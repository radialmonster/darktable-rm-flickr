#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>

#include "lua.h"
#include "lauxlib.h"

static wchar_t *utf8_to_wide(lua_State *L, const char *s)
{
  int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, NULL, 0);
  if(len <= 0) luaL_error(L, "utf8 conversion failed");
  wchar_t *w = (wchar_t *)lua_newuserdatauv(L, (size_t)len * sizeof(wchar_t), 0);
  if(!MultiByteToWideChar(CP_UTF8, 0, s, -1, w, len)) luaL_error(L, "utf8 conversion failed");
  return w;
}

static void push_win_error(lua_State *L, const char *prefix)
{
  DWORD code = GetLastError();
  lua_pushnil(L);
  lua_pushfstring(L, "%s failed (%d)", prefix, (int)code);
}

static int l_request(lua_State *L)
{
  const char *method8 = luaL_checkstring(L, 1);
  const char *url8 = luaL_checkstring(L, 2);
  size_t body_len = 0;
  const char *body = luaL_optlstring(L, 3, NULL, &body_len);
  const char *content_type8 = luaL_optstring(L, 4, NULL);

  wchar_t *url = utf8_to_wide(L, url8);
  wchar_t *method = utf8_to_wide(L, method8);

  URL_COMPONENTS parts;
  ZeroMemory(&parts, sizeof(parts));
  parts.dwStructSize = sizeof(parts);
  parts.dwSchemeLength = (DWORD)-1;
  parts.dwHostNameLength = (DWORD)-1;
  parts.dwUrlPathLength = (DWORD)-1;
  parts.dwExtraInfoLength = (DWORD)-1;

  if(!WinHttpCrackUrl(url, 0, 0, &parts)) {
    push_win_error(L, "WinHttpCrackUrl");
    return 2;
  }

  if(parts.nScheme != INTERNET_SCHEME_HTTP && parts.nScheme != INTERNET_SCHEME_HTTPS) {
    lua_pushnil(L);
    lua_pushliteral(L, "unsupported URL scheme");
    return 2;
  }

  wchar_t *host = (wchar_t *)lua_newuserdatauv(L, ((size_t)parts.dwHostNameLength + 1) * sizeof(wchar_t), 0);
  CopyMemory(host, parts.lpszHostName, (size_t)parts.dwHostNameLength * sizeof(wchar_t));
  host[parts.dwHostNameLength] = L'\0';

  DWORD path_len = parts.dwUrlPathLength + parts.dwExtraInfoLength;
  wchar_t *path = (wchar_t *)lua_newuserdatauv(L, ((size_t)path_len + 1) * sizeof(wchar_t), 0);
  CopyMemory(path, parts.lpszUrlPath, (size_t)parts.dwUrlPathLength * sizeof(wchar_t));
  if(parts.dwExtraInfoLength > 0) {
    CopyMemory(path + parts.dwUrlPathLength, parts.lpszExtraInfo, (size_t)parts.dwExtraInfoLength * sizeof(wchar_t));
  }
  path[path_len] = L'\0';

  HINTERNET session = WinHttpOpen(L"dtrmflickr-darktable-lua/1.0",
    WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if(!session) {
    push_win_error(L, "WinHttpOpen");
    return 2;
  }
  WinHttpSetTimeouts(session, 15000, 15000, 30000, 30000);

  HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
  if(!connect) {
    WinHttpCloseHandle(session);
    push_win_error(L, "WinHttpConnect");
    return 2;
  }

  DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
  HINTERNET request = WinHttpOpenRequest(connect, method, path, NULL,
    WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
  if(!request) {
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    push_win_error(L, "WinHttpOpenRequest");
    return 2;
  }

  wchar_t *headers = L"User-Agent: dtrmflickr-darktable-lua\r\n";
  DWORD headers_len = (DWORD)-1;
  if(content_type8 && content_type8[0]) {
    size_t hlen = strlen("User-Agent: dtrmflickr-darktable-lua\r\nContent-Type: \r\n") + strlen(content_type8);
    char *header8 = (char *)lua_newuserdatauv(L, hlen + 1, 0);
    snprintf(header8, hlen + 1, "User-Agent: dtrmflickr-darktable-lua\r\nContent-Type: %s\r\n", content_type8);
    headers = utf8_to_wide(L, header8);
  }

  LPVOID send_body = body ? (LPVOID)body : WINHTTP_NO_REQUEST_DATA;
  DWORD send_len = body ? (DWORD)body_len : 0;
  BOOL ok = WinHttpSendRequest(request, headers, headers_len, send_body, send_len, send_len, 0)
    && WinHttpReceiveResponse(request, NULL);
  if(!ok) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    push_win_error(L, "WinHttpSendRequest");
    return 2;
  }

  /* Surface the HTTP status. Flickr REST/upload always answer HTTP 200 even for
     API-level errors (the <err> lives in the response body), so any status >= 400
     here is a genuine transport/proxy/CDN/gateway failure, not a Flickr API error.
     Without this check a 502/503/504/429 would return the gateway's HTML error
     page as a "success" body and the plugin's queue would classify it
     non-retryable, defeating the documented gateway-retry. Return it as an
     "HTTP NNN" string so default_retryable (queue.lua) can retry it. */
  DWORD status = 0, status_size = sizeof(status);
  if(WinHttpQueryHeaders(request,
       WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
       WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX)
     && status >= 400) {
    WinHttpCloseHandle(request);
    WinHttpCloseHandle(connect);
    WinHttpCloseHandle(session);
    lua_pushnil(L);
    lua_pushfstring(L, "HTTP %d", (int)status);
    return 2;
  }

  luaL_Buffer b;
  luaL_buffinit(L, &b);
  for(;;) {
    DWORD available = 0;
    if(!WinHttpQueryDataAvailable(request, &available)) {
      WinHttpCloseHandle(request);
      WinHttpCloseHandle(connect);
      WinHttpCloseHandle(session);
      push_win_error(L, "WinHttpQueryDataAvailable");
      return 2;
    }
    if(available == 0) break;
    char *buf = luaL_prepbuffsize(&b, available);
    DWORD read = 0;
    if(!WinHttpReadData(request, buf, available, &read)) {
      WinHttpCloseHandle(request);
      WinHttpCloseHandle(connect);
      WinHttpCloseHandle(session);
      push_win_error(L, "WinHttpReadData");
      return 2;
    }
    luaL_addsize(&b, read);
  }

  WinHttpCloseHandle(request);
  WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);

  luaL_pushresult(&b);
  return 1;
}

/* Open a URL (or any shell target) in the default handler WITHOUT allocating a
   console. darktable is a GUI process with no console, so shelling out through
   `explorer "url"` / `cmd /c` (as `dt.control.execute` does) makes Windows
   allocate a fresh console for the child that briefly flashes on screen (#83).
   ShellExecuteW performs the same "open in default browser" action in-process,
   with no console. Returns true on success, or nil + reason on failure so the
   Lua caller can fall back to the shell-out path. */
static int l_shell_open(lua_State *L)
{
  const char *target8 = luaL_checkstring(L, 1);
  wchar_t *target = utf8_to_wide(L, target8);
  /* ShellExecuteW returns a value > 32 on success; <= 32 is an error code. */
  HINSTANCE rc = ShellExecuteW(NULL, L"open", target, NULL, NULL, SW_SHOWNORMAL);
  if((INT_PTR)rc <= 32) {
    lua_pushnil(L);
    lua_pushfstring(L, "ShellExecute failed (%d)", (int)(INT_PTR)rc);
    return 2;
  }
  lua_pushboolean(L, 1);
  return 1;
}

/* Set the clipboard to a UTF-8 string via the Win32 clipboard API, again with
   no console (unlike piping a temp file to `clip` through cmd.exe, #83).
   Returns true on success, or nil + reason on failure for the caller's
   fallback. On SetClipboardData success the system owns the global handle, so
   it must NOT be freed; on any earlier failure we free it ourselves. */
static int l_clipboard_set(lua_State *L)
{
  const char *text8 = luaL_checkstring(L, 1);
  int wlen = MultiByteToWideChar(CP_UTF8, 0, text8, -1, NULL, 0);
  if(wlen <= 0) {
    lua_pushnil(L);
    lua_pushliteral(L, "utf8 conversion failed");
    return 2;
  }
  HGLOBAL hmem = GlobalAlloc(GMEM_MOVEABLE, (SIZE_T)wlen * sizeof(wchar_t));
  if(!hmem) {
    push_win_error(L, "GlobalAlloc");
    return 2;
  }
  wchar_t *dst = (wchar_t *)GlobalLock(hmem);
  if(!dst) {
    GlobalFree(hmem);
    push_win_error(L, "GlobalLock");
    return 2;
  }
  MultiByteToWideChar(CP_UTF8, 0, text8, -1, dst, wlen);
  GlobalUnlock(hmem);

  if(!OpenClipboard(NULL)) {
    GlobalFree(hmem);
    push_win_error(L, "OpenClipboard");
    return 2;
  }
  EmptyClipboard();
  if(!SetClipboardData(CF_UNICODETEXT, hmem)) {
    CloseClipboard();
    GlobalFree(hmem); /* ownership not transferred on failure */
    push_win_error(L, "SetClipboardData");
    return 2;
  }
  CloseClipboard(); /* clipboard now owns hmem; do not free it */
  lua_pushboolean(L, 1);
  return 1;
}

static int open_module(lua_State *L)
{
  lua_newtable(L);
  lua_pushcfunction(L, l_request);
  lua_setfield(L, -2, "request");
  lua_pushcfunction(L, l_shell_open);
  lua_setfield(L, -2, "shell_open");
  lua_pushcfunction(L, l_clipboard_set);
  lua_setfield(L, -2, "clipboard_set");
  return 1;
}

int __declspec(dllexport) luaopen_dtrmflickr_winhttp(lua_State *L)
{
  return open_module(L);
}

int __declspec(dllexport) luaopen_dtrmflickr_dtrmflickr_winhttp(lua_State *L)
{
  return open_module(L);
}
