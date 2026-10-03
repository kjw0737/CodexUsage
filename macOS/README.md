# CodexUsage for macOS

Windows MFC 위젯과 같은 Codex 사용량 정보를 macOS 메뉴 막대와 작은 제목 없는 창에 표시합니다. 단기·장기 남은 비율, 리셋 시각·크레딧, 다음 조회 카운트다운, Warning/Alert 색상과 회복 알림을 지원합니다. 자동 밝음/어두움 테마, 투명도, 항상 위, 조회 주기, 로그인 시 실행, 시작 시 메뉴 막대에 숨기기도 설정할 수 있습니다.

## 준비사항

- macOS 13 이상, Xcode 15 이상과 Command Line Tools
- ChatGPT 계정으로 로그인한 Codex CLI (`codex`)
- GUI 버튼을 사용하려면 macOS Codex 앱

## 빌드 및 사용

[최신 macOS 범용 앱 ZIP 다운로드](https://github.com/kjw0737/CodexUsage/raw/refs/heads/main/dist/CodexUsageMac-1.0.2026.0927-universal.zip). 압축을 풀어 CodexUsageMac.app을 /Applications로 옮겨 실행하세요. 빌드 없이 사용할 수 있습니다. 로컬 실행용 ad-hoc 서명이며 공증된 배포본은 아닙니다.

Mac에서 저장소를 받은 뒤 다음 명령을 실행합니다.

    cd macOS
    chmod +x build-app.sh
    ./build-app.sh

스크립트가 `swift test`, Apple Silicon·Intel 범용 Release 빌드, 아이콘 생성, 로컬 실행을 위한 ad-hoc 서명을 수행하고 `macOS/dist/CodexUsageMac.app`을 만듭니다. 앱을 `/Applications`로 옮긴 뒤 실행하세요. 처음 시작할 때 알림 권한을 허용하면 기준 도달·회복 알림을 받을 수 있습니다. 앱이 처음 실행될 때는 창과 메뉴 막대 아이콘이 보입니다. `−` 버튼은 창만 숨기고, 메뉴 막대 아이콘을 클릭하면 복원합니다. 우클릭 메뉴에는 복원·새로 고침·종료가 있습니다.

`Settings`에서 투명도(20~100%), 조회 주기(10~3600초), Warning/Alert 남은 비율(`0 ≤ Alert < Warning ≤ 100`), 항상 위, 로그인 시 실행과 메뉴 막대 시작 여부를 설정합니다. Save usage data to CSV를 켜면 정상 조회 때마다 ~/Library/Application Support/CodexUsage/usageYYYYMMDD.csv에 날짜·시간과 5시간·주간 남은 비율을 기록합니다. CLI나 GUI 자동 검색이 실패하면 전체 경로를 지정하세요. 로그인 항목은 macOS 시스템 설정에서 승인이 필요할 수 있습니다. CLI 버튼은 Terminal에서 대화형 Codex를 열고, GUI 버튼은 실행 중인 Codex 앱을 표시하거나 실행합니다.

사용량은 CLI의 App Server `account/rateLimits/read` 응답을 읽습니다. `usedPercent`를 남은 비율로 바꾸고, Codex 기본 한도가 없으면 다른 모델의 한도로 대체하지 않습니다. 토큰은 별도 저장하지 않습니다.

## 사용량 기록 그래프

위젯의 Refresh 옆 그래프 버튼을 누르면 CSV 기록을 읽는 창이 열립니다. 기록이 꺼져 있으면 먼저 설정에서 켜라는 메시지를 표시합니다. 그래프에서 1시간·5시간·1일·7일·30일 범위를 선택하고 첫·이전·다음·마지막 버튼으로 기록이 있는 구간을 이동할 수 있습니다. 기본 범위는 5시간입니다. 25·50·75% 기준선과 각 기록점의 측정 시각·잔여량 도움말을 표시하고, 5시간 잔여량이 100% 미만에서 100%로 회복된 실제 측정 지점을 리셋 세로선으로 표시합니다. 창 크기를 조절하면 그래프도 새 크기에 맞춰 다시 그리며 macOS 테마에 따라 색이 바뀝니다.

History 창의 **Token stats** 버튼은 `~/.codex/sessions`의 로컬 JSONL 세션을 읽어 모델별 토큰 사용량을 별도 창에 표시합니다. 명령 1회당 평균 입력은 순수 미캐시·캐시 입력으로, 출력은 일반·추론 출력으로 나누어 서로 다른 스케일의 막대로 비교합니다. 모델별 호출 수·캐시율·LOW SAMPLE 표시와 최소–최대 범위선·중앙값 틱을 제공하며, 아래 텍스트 영역에서 상세 통계와 CSV 한도 감소 참고값을 확인할 수 있습니다.

## 검증 상태

기존 소스는 GitHub Actions의 Mac 러너에서 Apple Silicon·Intel 범용 앱 빌드와 CSV 기능을 포함한 7개 Swift 테스트가 통과했습니다. 이번 변경에는 그래프 범위·리셋 판정과 실제 세션 형식의 토큰 통계를 검증하는 테스트 2개를 추가했습니다. 현재 작업 환경은 Windows라 새 테스트와 범용 앱 빌드는 Mac 러너에서 확인해야 합니다. [macOS 미리보기 ZIP](https://github.com/kjw0737/CodexUsage/releases/tag/v1.0.2026.0926-mac-preview)을 받을 수 있습니다. 이 기존 미리보기 ZIP에는 CSV 그래프와 Token stats 기능이 포함되지 않습니다. 실제 사용자 세션에서 화면, CLI 연결, 알림 권한, 로그인 항목 동작은 추가 검증이 필요합니다. 배포용으로 공증된 DMG는 아직 제공하지 않습니다.

창의 버튼이 아닌 배경과 텍스트 영역을 드래그하면 창을 이동할 수 있습니다.

2026-09-27 00:10: macOS 빌드에서 설치된 Xcode를 우선 사용하도록 수정했습니다. SwiftUI 매크로 누락 오류를 피하고, Apple Silicon·Intel 빌드 폴더를 분리해 실행 파일 덮어쓰기를 방지합니다. 사용자 지정 Xcode는 DEVELOPER_DIR로 선택할 수 있습니다.

2026-09-27 00:15: macOS 앱 아이콘(ICNS) 생성 원본을 CodexUsage/res/CodexUsage.png로 변경했습니다.

2026-09-27 00:43: macOS 위젯의 버튼 외 영역에서 네이티브 창 드래그를 시작하도록 수정했습니다. 버튼의 실제 배치 영역은 클릭이 통과하도록 제외합니다.

2026-10-03: macOS History에 5시간·7일 범위와 실제 잔여량 회복 리셋선을 추가하고 기본 범위를 5시간으로 변경했습니다. 로컬 Codex 세션을 분석하는 모델별 Token stats 창, 입력·출력 듀얼 막대, 캐시율·표본 수·중앙값·범위 표시와 관련 Swift 테스트를 추가했습니다.
