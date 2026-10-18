"""OBS WebSocket(v5)으로 녹화를 조작하는 작은 도구 (파이썬 표준 라이브러리만 · 인증 끈 OBS 전용).

Game Bar는 '맨 앞 창'만 녹화하지만, OBS '창 캡처'는 다른 창 뒤에 있는 게임 창도 녹화하고
창 크기·모드가 바뀌어도 끊기지 않는다 → 사용자가 PC를 쓰는 동안에도 녹화할 수 있다.

준비 (사용자가 한 번):
  OBS → 도구 → WebSocket 서버 설정 → 'WebSocket 서버 사용' 켜기 · '인증 기능 사용' 끄기 (비밀번호는 다루지 않음)

사용법 (Tools/UnrealAgent 폴더에서):
  python obs_ws.py status                      # 연결 확인 · 녹화 중인지 · 녹화 폴더
  python obs_ws.py setup "<창 제목 시작>" <exe>  # 'CozyRealm 녹화' 장면 + 창 캡처를 만들고(있으면 고침) 그 장면으로 전환
  python obs_ws.py start | stop                # 녹화 시작 · 중지 (stop은 저장된 파일 경로를 출력)
  python obs_ws.py restore                     # setup 전에 보던 장면으로 되돌림
상태 파일: .state/obs_prev_scene.txt (setup 전 장면 이름)
"""
import base64
import json
import os
import socket
import struct
import sys
import uuid

HOST, PORT = '127.0.0.1', int(os.environ.get('OBS_WS_PORT', '4455'))
SCENE = 'CozyRealm 녹화'
SOURCE = 'CozyRealm 게임 창'
STATE = os.path.join(os.path.dirname(os.path.abspath(__file__)), '.state')


class Ws:
    def __init__(self):
        self.s = socket.create_connection((HOST, PORT), timeout=10)
        key = base64.b64encode(os.urandom(16)).decode()
        self.s.sendall((('GET / HTTP/1.1\r\nHost: %s:%d\r\nUpgrade: websocket\r\nConnection: Upgrade\r\n'
                        'Sec-WebSocket-Key: %s\r\nSec-WebSocket-Version: 13\r\nSec-WebSocket-Protocol: obswebsocket.json\r\n\r\n') % (HOST, PORT, key)).encode())
        head = b''
        while b'\r\n\r\n' not in head:
            head += self.s.recv(1)
        if b' 101 ' not in head.split(b'\r\n')[0]:
            raise RuntimeError('WebSocket 연결 실패: ' + head.decode(errors='replace'))
        hello = self.recv()
        if hello.get('d', {}).get('authentication'):
            raise RuntimeError('OBS 인증이 켜져 있습니다 (이 도구는 비밀번호를 다루지 않음) · 도구 → WebSocket 서버 설정에서 인증을 끄세요')
        self.send({'op': 1, 'd': {'rpcVersion': 1, 'eventSubscriptions': 0}})
        if self.recv().get('op') != 2:
            raise RuntimeError('OBS Identify 실패')

    def send(self, obj):
        data = json.dumps(obj).encode()
        mask = os.urandom(4)
        n = len(data)
        head = bytearray([0x81])
        if n < 126:
            head.append(0x80 | n)
        elif n < 65536:
            head.append(0x80 | 126); head += struct.pack('>H', n)
        else:
            head.append(0x80 | 127); head += struct.pack('>Q', n)
        self.s.sendall(bytes(head) + mask + bytes(b ^ mask[i % 4] for i, b in enumerate(data)))

    def _read(self, n):
        buf = b''
        while len(buf) < n:
            chunk = self.s.recv(n - len(buf))
            if not chunk:
                raise RuntimeError('OBS 연결이 끊겼습니다')
            buf += chunk
        return buf

    def recv(self):
        while True:
            b1, b2 = self._read(2)
            n = b2 & 0x7F
            if n == 126:
                n = struct.unpack('>H', self._read(2))[0]
            elif n == 127:
                n = struct.unpack('>Q', self._read(8))[0]
            payload = self._read(n)
            if (b1 & 0x0F) == 1:
                return json.loads(payload.decode())

    def req(self, kind, data=None):
        rid = str(uuid.uuid4())
        self.send({'op': 6, 'd': {'requestType': kind, 'requestId': rid, 'requestData': data or {}}})
        while True:
            m = self.recv()
            if m.get('op') == 7 and m['d'].get('requestId') == rid:
                st = m['d']['requestStatus']
                if not st.get('result'):
                    raise RuntimeError('%s 실패: %s' % (kind, st.get('comment') or st.get('code')))
                return m['d'].get('responseData', {})


def find_window_title(prefix):
    """지금 떠 있는 언리얼 창 중 제목이 prefix로 시작하는 창의 전체 제목 (없으면 None)"""
    import ctypes
    from ctypes import wintypes
    u = ctypes.windll.user32
    found = []

    @ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
    def cb(h, _):
        n = u.GetWindowTextLengthW(h)
        if n and u.IsWindowVisible(h):
            b = ctypes.create_unicode_buffer(n + 1); u.GetWindowTextW(h, b, n + 1)
            if b.value.startswith(prefix):
                found.append(b.value)
        return True
    u.EnumWindows(cb, 0)
    return found[0] if found else None


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'status'
    ws = Ws()
    if cmd == 'status':
        rec = ws.req('GetRecordStatus')
        print('연결됨 · 녹화 중=%s · 녹화 폴더=%s · 지금 장면=%s' % (rec.get('outputActive'), ws.req('GetRecordDirectory').get('recordDirectory'),
                                                     ws.req('GetCurrentProgramScene').get('currentProgramSceneName')))
    elif cmd == 'setup':
        title, exe = sys.argv[2], sys.argv[3]
        os.makedirs(STATE, exist_ok=True)
        cur = ws.req('GetCurrentProgramScene').get('currentProgramSceneName')
        if cur != SCENE:
            open(os.path.join(STATE, 'obs_prev_scene.txt'), 'w', encoding='utf-8').write(cur)
        scenes = [s['sceneName'] for s in ws.req('GetSceneList')['scenes']]
        if SCENE not in scenes:
            ws.req('CreateScene', {'sceneName': SCENE})
        # 창 지정: '제목:창 클래스:exe' · priority 0 = 제목이 같은 창만 (2 = 같은 exe면 아무 창이나 →
        # PIE가 꺼지면 에디터 본 창을 찍어 버림 · 2026-10-18) · 제목은 지금 떠 있는 창에서 시작 글자로 찾아 정확히 넣음
        full = find_window_title(title) or title
        settings = {'window': '%s:UnrealWindow:%s' % (full.replace(':', '#3A'), exe), 'priority': 0, 'method': 2, 'cursor': False, 'client_area': True}
        title = full
        inputs = [i['inputName'] for i in ws.req('GetInputList')['inputs']]
        if SOURCE in inputs:
            ws.req('SetInputSettings', {'inputName': SOURCE, 'inputSettings': settings, 'overlay': True})
        else:
            ws.req('CreateInput', {'sceneName': SCENE, 'inputName': SOURCE, 'inputKind': 'window_capture', 'inputSettings': settings})
        # 화면에 꽉 차게 (창 크기가 바뀌어도 비율 유지)
        item = ws.req('GetSceneItemId', {'sceneName': SCENE, 'sourceName': SOURCE})['sceneItemId']
        video = ws.req('GetVideoSettings')
        ws.req('SetSceneItemTransform', {'sceneName': SCENE, 'sceneItemId': item, 'sceneItemTransform': {
            'boundsType': 'OBS_BOUNDS_SCALE_INNER', 'boundsWidth': video['baseWidth'], 'boundsHeight': video['baseHeight'],
            'positionX': 0, 'positionY': 0, 'alignment': 5, 'boundsAlignment': 0}})
        ws.req('SetCurrentProgramScene', {'sceneName': SCENE})
        print('준비됨: 장면 "%s" · 창 캡처 %s (%s) · 이전 장면 "%s"' % (SCENE, title, exe, cur))
    elif cmd == 'start':
        ws.req('StartRecord')
        print('녹화 시작')
    elif cmd == 'stop':
        out = ws.req('StopRecord')
        print('녹화 중지 · 파일=%s' % out.get('outputPath'))
    elif cmd == 'restore':
        p = os.path.join(STATE, 'obs_prev_scene.txt')
        if os.path.exists(p):
            prev = open(p, encoding='utf-8').read().strip()
            ws.req('SetCurrentProgramScene', {'sceneName': prev})
            print('장면 되돌림: "%s"' % prev)
        else:
            print('되돌릴 장면 기록 없음')


if __name__ == '__main__':
    try:
        sys.stdout.reconfigure(encoding='utf-8')
    except Exception:
        pass
    try:
        main()
    except (OSError, RuntimeError) as e:
        print('실패: %s' % e)
        sys.exit(2)
