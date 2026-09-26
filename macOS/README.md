# CodexUsage for macOS

Windows MFC 위젯과 같은 Codex 사용량 정보를 macOS 메뉴 막대와 작은 제목 없는 창에 표시합니다. 단기·장기 남은 비율, 리셋 시각·크레딧, 다음 조회 카운트다운, Warning/Alert 색상과 회복 알림을 지원합니다. 자동 밝음/어두움 테마, 투명도, 항상 위, 조회 주기, 로그인 시 실행, 시작 시 메뉴 막대에 숨기기도 설정할 수 있습니다.

## 준비사항

- macOS 13 이상, Xcode 15 이상과 Command Line Tools
- ChatGPT 계정으로 로그인한 Codex CLI (`codex`)
- GUI 버튼을 사용하려면 macOS Codex 앱

## 빌드 및 사용

Mac에서 저장소를 받은 뒤 다음 명령을 실행합니다.

    cd macOS
    chmod +x build-app.sh
    ./build-app.sh

스크립트가 `swift test`, Release 빌드, 아이콘 생성, 로컬 실행을 위한 ad-hoc 서명을 수행하고 `macOS/dist/CodexUsageMac.app`을 만듭니다. 앱을 `/Applications`로 옮긴 뒤 실행하세요. 처음 시작할 때 알림 권한을 허용하면 기준 도달·회복 알림을 받을 수 있습니다. 앱이 처음 실행될 때는 창과 메뉴 막대 아이콘이 보입니다. `−` 버튼은 창만 숨기고, 메뉴 막대 아이콘을 클릭하면 복원합니다. 우클릭 메뉴에는 복원·새로 고침·종료가 있습니다.

`Settings`에서 투명도(20~100%), 조회 주기(10~3600초), Warning/Alert 남은 비율(`0 ≤ Alert < Warning ≤ 100`), 항상 위, 로그인 시 실행과 메뉴 막대 시작 여부를 설정합니다. CLI나 GUI 자동 검색이 실패하면 전체 경로를 지정하세요. 로그인 항목은 macOS 시스템 설정에서 승인이 필요할 수 있습니다. CLI 버튼은 Terminal에서 대화형 Codex를 열고, GUI 버튼은 실행 중인 Codex 앱을 표시하거나 실행합니다.

사용량은 CLI의 App Server `account/rateLimits/read` 응답을 읽습니다. `usedPercent`를 남은 비율로 바꾸고, Codex 기본 한도가 없으면 다른 모델의 한도로 대체하지 않습니다. 토큰은 별도 저장하지 않습니다.

## 검증 상태

Swift 테스트와 `.app` 빌드 명령은 스크립트에 포함되어 있습니다. 이 저장소의 macOS 코드는 Windows에서 작성했으므로 실제 Mac 빌드, 알림 권한, 로그인 항목 및 앱 실행 검증이 필요합니다. 배포용으로 공증된 DMG는 아직 제공하지 않습니다.
