# UnrealAgent 도구 — AI가 언리얼 에디터를 조작·검증·녹화할 때 쓰는 스크립트

Claude Code·Codex 같은 AI 채팅이 CozyRealm 언리얼 에디터에 연결해 다음 일을 할 때 쓰는 도구 모음입니다.
- 플레이(PIE)를 시작하고 끕니다.
- 화면을 캡처하고 로그를 읽습니다.
- 바뀐 C++ 코드를 Live Coding으로 반영합니다.
- 게임 UI 버튼을 실제로 클릭합니다.
- 화면을 녹화하고, 녹화 파일에 게임 화면이 담겼는지 확인합니다.

임시 폴더나 특정 채팅에 의존하지 않습니다. 이 폴더에 있는 파일만으로 동작합니다.

## 한눈에 보기

| 하는 일 | 쓰는 것 | 파일 |
|---|---|---|
| 에디터 조작 (PIE·캡처·로그·Live Coding) | 언리얼 MCP 서버 (HTTP) | `mcp_call.py` |
| 게임 화면 클릭·키 입력 | Windows 입력 (user32.dll) + 보호 기능 | `ue_input.ps1`, `ue_steps.ps1` |
| 녹화 시작·중지 | Xbox Game Bar (`Win+Alt+R`) + 보호 기능 | `record_toggle.ps1` |
| 녹화 파일 옮기기·이름 바꾸기 | PowerShell | `save_recording.ps1` |
| 영상 장면 추출 (녹화 확인) | Windows 기본 WinRT 영상 기능 | `video_frame.ps1` |
| 캡처 일부 확대 | System.Drawing | `crop_image.ps1` |
| 보호 기능 공통 함수 | — | `_UeGuard.ps1` |

따로 설치할 프로그램은 없습니다. 필요한 것은 Windows 10/11, Windows PowerShell 5.1(기본 포함), Python 3입니다.

---

## 1. 언리얼 쪽 설정 (한 번만)

### 플러그인
아래 플러그인은 모두 UE 5.8 기본 플러그인이고, `CozyRealm/CozyRealm.uproject`에 이미 켜져 있습니다.

| 플러그인 | 역할 |
|---|---|
| `ModelContextProtocol` | 에디터 안에서 MCP 서버를 실행합니다 |
| `ToolsetRegistry` | 도구 묶음을 등록하고 찾게 해 줍니다 |
| `EditorToolset` | PIE 시작·종료, 화면 캡처, 로그 읽기 |
| `LiveCodingToolset` | Live Coding 컴파일 |
| `UMGToolSet`, `ConfigSettingsToolset`, `AutomationTestToolset`, `AllToolsets` | 그 밖의 도구 묶음 |

### MCP 서버 설정
- 에디터 메뉴: **편집 → 에디터 개인설정 → Model Context Protocol**
- 실제 저장 위치: `CozyRealm/Saved/Config/WindowsEditor/EditorPerProjectUserSettings.ini` (개인 설정이라 Git에 포함되지 않습니다)

```ini
[/Script/ModelContextProtocolEngine.ModelContextProtocolSettings]
ServerUrlPath=/mcp
ServerPortNumber=8000
bAutoStartServer=True
bEnableToolSearch=True
```

- 에디터를 켜면 `http://127.0.0.1:8000/mcp` 주소로 서버가 자동으로 시작됩니다. 출력 로그에 `Starting MCP server on port 8000`이 보이면 정상입니다.
- 비밀번호나 인증키는 없습니다. 내 컴퓨터(127.0.0.1)에서만 접속할 수 있습니다.
- 포트를 바꾸려면 위 설정을 고치거나, 에디터를 `-ModelContextProtocolPort=N` 옵션으로 실행합니다.

---

## 2. AI 채팅에서 연결하기

### Claude Code
저장소 루트의 `.mcp.json`에 이미 들어 있습니다.
```json
{ "mcpServers": { "unreal-mcp": { "type": "http", "url": "http://127.0.0.1:8000/mcp" } } }
```

### Codex
`~/.codex/config.toml`에 추가합니다. Codex 버전마다 문법이 다를 수 있으니, 안 되면 해당 버전의 MCP 설정 문서를 확인하세요.
```toml
# HTTP 주소를 직접 지원하는 버전
[mcp_servers.unreal-mcp]
url = "http://127.0.0.1:8000/mcp"

# 지원하지 않는 버전: 중계 프로그램(mcp-remote, Node.js 필요)을 거칩니다
# [mcp_servers.unreal-mcp]
# command = "npx"
# args = ["-y", "mcp-remote", "http://127.0.0.1:8000/mcp"]
```

### 연결이 안 될 때: `mcp_call.py`로 직접 부르기
채팅을 시작할 때 에디터가 꺼져 있었다면, 그 채팅의 MCP 연결은 실패한 상태로 남습니다(`ECONNREFUSED`). 이럴 때는 `mcp_call.py`로 같은 서버를 직접 부르면 됩니다. MCP 설정 없이 Python만 있으면 됩니다.

서버가 처음에 보여 주는 도구는 메타 도구(`list_toolsets`, `call_tool` 등)뿐입니다. 실제 기능은 `call_tool`에 **도구 묶음 이름 + 도구 이름**을 넣어 부릅니다.

| 용도 | 도구 묶음 (`toolset_name`) | 도구 (`tool_name`) |
|---|---|---|
| 플레이 시작·종료 | `EditorToolset.EditorAppToolset` | `StartPIE`, `StopPIE` |
| 에디터 화면 캡처 | `EditorToolset.EditorAppToolset` | `CaptureEditorImage` |
| 로그 읽기 | `EditorToolset.LogsToolset` | `GetLogEntries` |
| C++ 바로 반영 | `LiveCodingToolset.LiveCodingToolset` | `CompileLiveCoding` |

---

## 3. 사용법

아래 예시는 모두 이 폴더(`Tools/UnrealAgent`)에서 실행합니다. 다른 위치에서 실행하면 파일 경로를 붙이세요.

### 에디터 조작 — `mcp_call.py`
```powershell
python mcp_call.py toolsets                      # 도구 묶음 목록
python mcp_call.py pie start                     # 플레이 시작 (뷰포트 안에서)
python mcp_call.py capture D:\shots\01.png       # 에디터 화면 캡처 (1280×720)
python mcp_call.py logs LogCozyRealm "업그레이드"  # 로그 중 정규식에 맞는 줄
python mcp_call.py livecoding                    # 헤더를 안 바꾼 .cpp 수정 반영
python mcp_call.py pie stop
python mcp_call.py call EditorToolset.EditorAppToolset StartPIE '{"options":{"playMode":"PlayMode_InViewPort"}}'
```
- 다른 주소를 쓰려면 환경 변수 `UNREAL_MCP_URL`을 지정합니다.
- 세션 아이디는 이 폴더의 `.state/`에 저장됩니다(Git에서 제외). 에디터를 다시 켜면 자동으로 새 세션을 만듭니다.
- Live Coding은 디스크가 느리면 오래 걸립니다(2026-10-07 E: 드라이브에서 약 20분). 컴파일러가 멈춘 것처럼 보여도 디스크를 기다리는 중일 수 있습니다.
- 헤더(.h)를 바꿨다면 Live Coding으로는 반영할 수 없습니다. 에디터를 끄고 빌드한 뒤 다시 켜야 합니다.
  ```
  "C:\Program Files\Epic Games\UE_5.8\Engine\Build\BatchFiles\Build.bat" CozyRealmEditor Win64 Development -Project="E:\Project\UE5_CozyRealm\CozyRealm\CozyRealm.uproject" -NoUBA -WaitMutex
  ```

### 게임 화면 클릭·키 — `ue_input.ps1`, `ue_steps.ps1`
```powershell
powershell -ExecutionPolicy Bypass -File ue_input.ps1 check            # 입력 없이 대상 창 확인만
powershell -ExecutionPolicy Bypass -File ue_input.ps1 click 500 420    # 클릭
powershell -ExecutionPolicy Bypass -File ue_input.ps1 key F1           # 키 (F1, Enter, Space, 숫자 코드 …)

# 여러 단계를 한 번에 이어서 실행 · 하나라도 막히면 거기서 멈춤
powershell -ExecutionPolicy Bypass -File ue_steps.ps1 "click 500 420" "key F1" "wait 1" "cap 02.png" -CaptureDir D:\shots
```
- **좌표 기준**: `mcp_call.py capture`로 찍은 1280×720 캡처 이미지의 픽셀 좌표를 그대로 씁니다. 실제 창 크기에 맞춰 자동으로 바꿔 줍니다.
- 시간에 민감한 조작은 반드시 `ue_steps.ps1` 한 번으로 이어서 실행합니다(예: 가공을 시작하자마자 주민을 빼서 정지). 도구 호출을 나누면 AI가 응답하는 사이 몇 초가 지나 게임 상태가 바뀝니다.
- 창 크기가 바뀌면 버튼 위치도 바뀝니다. 클릭 전에 다시 캡처해서 좌표를 확인하세요.

### 녹화 — `record_toggle.ps1`, `save_recording.ps1`, `video_frame.ps1`
```powershell
powershell -ExecutionPolicy Bypass -File record_toggle.ps1        # 녹화 시작
# … 조작 …
powershell -ExecutionPolicy Bypass -File record_toggle.ps1        # 녹화 중지
powershell -ExecutionPolicy Bypass -File save_recording.ps1 -Name "2026-10-07_기능3_내용_01.mp4"
powershell -ExecutionPolicy Bypass -File video_frame.ps1 -Video "<저장된 경로>" -Seconds "3,30,60" -OutDir D:\frames
```
- Xbox Game Bar가 켜져 있어야 합니다(Windows 설정 → 게임 → Xbox Game Bar). 녹화 파일은 `동영상\Captures`에 먼저 생깁니다.
- `save_recording.ps1`은 방금 만든 녹화가 다 써질 때까지 기다린 뒤 `바탕 화면\CozyRealm\Reocode`로 옮깁니다. 이 폴더가 없으면 원래 폴더에 둡니다. 같은 이름의 파일이 있으면 덮어쓰지 않고 멈춥니다.
- **녹화를 완료로 기록하기 전에** `video_frame.ps1`로 장면을 뽑아 게임 화면이 담겼는지 확인합니다. 길이가 0이거나 Claude·Codex 창이 찍혔으면 실패입니다.
- 한글 파일명과 경로는 bash에서 PowerShell로 넘길 때 깨집니다. 녹화·영상 스크립트는 **PowerShell에서 직접** 실행하세요.

### 캡처 확대 — `crop_image.ps1`
```powershell
powershell -ExecutionPolicy Bypass -File crop_image.ps1 -Src 02.png -X 318 -Y 236 -Width 390 -Height 46 -Out zoom.png -Scale 3
```

---

## 4. 보호 기능 (입력 전 확인)

클릭·키 입력·녹화(`ue_input.ps1`, `ue_steps.ps1`, `record_toggle.ps1`)는 매번 아래 순서로 확인합니다. 하나라도 맞지 않으면 **아무 입력도 보내지 않고** 멈춥니다.

1. 창 제목이 `<프로젝트> - Unreal Editor`인 UnrealEditor 창을 찾습니다. 프로젝트 이름은 `-Project` 옵션, 환경 변수 `UE_PROJECT_NAME`, 기본값 `CozyRealm` 순서로 정합니다.
   - 창이 없으면 `NOTFOUND`(종료 코드 3)로 멈춥니다.
   - 후보가 여러 개면 `AMBIGUOUS`(4)로 멈춥니다.
2. 그 창을 맨 앞으로 가져온 뒤, **실제로 맨 앞인지 다시 확인**합니다.
   - 아니면 `BLOCKED`(2)로 멈추고, 그때 맨 앞이던 프로그램 이름을 알려 줍니다.
3. 확인이 끝났을 때만 입력을 보냅니다.

왜 필요한가:
- 사용자가 다른 프로그램(Blender, ChatGPT 등)을 쓰고 있으면 클릭이 그 프로그램으로 갈 수 있습니다.
- Game Bar는 맨 앞 창을 녹화하므로, 확인하지 않으면 AI 채팅 창이 녹화될 수 있습니다.
- 사용자가 다른 창을 쓰는 중이면 Windows가 창을 앞으로 가져오는 것을 막습니다. 그때는 `BLOCKED`가 나는 것이 정상입니다. 사용자에게 물어본 뒤 진행하세요.

---

## 5. 꼭 알아 둘 것

- **PIE 중 Esc는 게임 창이 아니라 PIE 자체를 끕니다.** 게임 안의 창은 화면의 '닫기' 버튼으로 닫으세요.
- 같은 에디터를 **두 채팅이 동시에 조작하지 마세요.** 클릭과 녹화가 서로 섞입니다.
- 캡처나 녹화를 정리할 때 와일드카드 삭제(`rm 4*_*.png` 등)는 쓰지 마세요. 지울 파일 이름을 하나씩 지정하세요(2026-10-07 원래 캡처를 실수로 지운 적이 있음).
- 영상은 로컬에만 둡니다. 노션·Git에는 올리지 않습니다(프로젝트 규칙 · `AGENTS.md`).
- 에디터 창 밖의 별도 창(떠 있는 애셋 편집기 등)은 녹화와 좌표 계산에 포함되지 않습니다.

## 6. 문제 해결

| 증상 | 원인과 해결 |
|---|---|
| `연결 실패 … 127.0.0.1:8000` | 에디터가 꺼져 있거나 MCP 서버가 시작되지 않았습니다. 출력 로그에서 `Starting MCP server`를 확인하세요 |
| `BLOCKED` | 다른 창이 맨 앞입니다. 사용자가 다른 작업 중이면 기다리거나 물어보세요 |
| `NOTFOUND` | 에디터 창 제목이 다릅니다. `-Project <이름>`으로 지정하세요 |
| 클릭이 엉뚱한 곳에 감 | 창 크기나 배치가 바뀌었습니다. 다시 캡처해서 좌표를 확인하세요 |
| 녹화 길이 0 / 다른 창이 찍힘 | Game Bar 설정을 확인하고, `record_toggle.ps1`로 다시 녹화하세요 |
| 한글이 깨져 보임 | PowerShell에서 직접 실행하세요. 스크립트는 UTF-8(BOM) 파일이고, 출력도 UTF-8로 맞춰 둡니다 |
