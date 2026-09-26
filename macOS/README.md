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

스크립트가 `swift test`, Apple Silicon·Intel 범용 Release 빌드, 아이콘 생성, 로컬 실행을 위한 ad-hoc 서명을 수행하고 `macOS/dist/CodexUsageMac.app`을 만듭니다. 앱을 `/Applications`로 옮긴 뒤 실행하세요. 처음 시작할 때 알림 권한을 허용하면 기준 도달·회복 알림을 받을 수 있습니다. 앱이 처음 실행될 때는 창과 메뉴 막대 아이콘이 보입니다. `−` 버튼은 창만 숨기고, 메뉴 막대 아이콘을 클릭하면 복원합니다. 우클릭 메뉴에는 복원·새로 고침·종료가 있습니다.

`Settings`에서 투명도(20~100%), 조회 주기(10~3600초), Warning/Alert 남은 비율(`0 ≤ Alert < Warning ≤ 100`), 항상 위, 로그인 시 실행과 메뉴 막대 시작 여부를 설정합니다. Save usage data to CSV를 켜면 정상 조회 때마다 ~/Library/Application Support/CodexUsage/usageYYYYMMDD.csv에 날짜·시간과 5시간·주간 남은 비율을 기록합니다. CLI나 GUI 자동 검색이 실패하면 전체 경로를 지정하세요. 로그인 항목은 macOS 시스템 설정에서 승인이 필요할 수 있습니다. CLI 버튼은 Terminal에서 대화형 Codex를 열고, GUI 버튼은 실행 중인 Codex 앱을 표시하거나 실행합니다.

사용량은 CLI의 App Server `account/rateLimits/read` 응답을 읽습니다. `usedPercent`를 남은 비율로 바꾸고, Codex 기본 한도가 없으면 다른 모델의 한도로 대체하지 않습니다. 토큰은 별도 저장하지 않습니다.

## 사용량 기록 그래프

위젯의 Refresh 옆 그래프 버튼을 누르면 CSV 기록을 읽는 창이 열립니다. 기록이 꺼져 있으면 먼저 설정에서 켜라는 메시지를 표시합니다. 그래프에서 1시간·1일·30일 범위를 선택하고 첫·이전·다음·마지막 버튼으로 기록이 있는 구간을 이동할 수 있습니다. 25·50·75% 기준선과 각 기록점의 측정 시각·잔여량 도움말을 표시합니다. macOS 테마에 따라 그래프 색도 바뀝니다.

## 검증 상태

기존 공개 버전은 GitHub Actions의 Mac 러너에서 Apple Silicon·Intel 범용 앱 빌드와 4개 Swift 테스트가 통과했습니다. 새 CSV 그래프 코드는 Mac에서 아직 빌드·실행 검증하지 못했습니다. [macOS 미리보기 ZIP](https://github.com/kjw0737/CodexUsage/releases/tag/v1.0.2026.0926-mac-preview)을 받을 수 있습니다. 이 기존 미리보기 ZIP에는 CSV 그래프 기능이 포함되지 않습니다. 실제 사용자 세션에서 화면, CLI 연결, 알림 권한, 로그인 항목 동작은 추가 검증이 필요합니다. 배포용으로 공증된 DMG는 아직 제공하지 않습니다.

창의 버튼이 아닌 배경과 텍스트 영역을 드래그하면 창을 이동할 수 있습니다.
