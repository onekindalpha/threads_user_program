# KAIST Pintos (x86-64) · Krafton Jungle SW-AI Lab 13기

| 프로젝트 | 기간 | 팀 | 결과 |
|---|---|---|---|
| Project 1: Threads | 2026.10 (7주차) | 2인 (김은기, 최무헌) | 27/27 PASS · 3개 환경 1,080회 반복 실행 전부 PASS |
| Project 2: User Programs | 2026.10.17 – 10.21 (8주차) | 2인 페어 (김은기, 임동호) | 95/95 PASS (최종 머지 기준) |

## Project 1: Threads

| 파트 | 한정된 자원을 ___하는 문제 | 문제 | 해결 |
|---|---|---|---|
| Alarm Clock | 낭비 | busy waiting | sleep / wake |
| Priority Scheduling | 순서 | 우선순위를 무시한 FIFO 실행 | 우선순위 정렬 + 선점 |
| Priority Donation | 공유 | 우선순위 역전 | 기부(중첩·연쇄 최대 8단계) |
| MLFQS | 공정 | 기아 | recent_cpu · nice 기반 재계산 |

**내 역할**
- Priority 테스트 13개를 직접 실행·분석해 요구사항을 규칙 R1~R7로 도출하고, 규칙별 근거 테스트·담당자·함수 변경사항을 정리한 설계 문서 작성
- Priority Donation 구현: 락 대기 시 주인에게 기부, 연쇄 기부, 락 해제 시 해당 락 몫만 회수 후 재계산
- MLFQS 테스트 분석 및 플로우차트 공동 설계·구현

**트러블슈팅**
- 빈 락 획득 시 `sema_down`이 건너뛰어져 한 락에 주인이 둘이 되는 PANIC → 획득 로직을 조건문 밖으로 이동
- 기부 후 원래 우선순위가 갱신되지 않던 문제 → `thread_set_priority`에서 원래 값 저장 후 재계산
- 실행 스크립트가 CRLF로 올라가 `source ./activate` 실패 → LF로 변환해 해결 (PR #37)

**검증**: 타이밍에 따라 결과가 달라질 수 있어 클라우드 Linux · macOS · Windows(WSL2) 3개 환경에서 18개 테스트를 20회씩, 총 1,080회 반복 실행해 전부 통과 확인

**협업**: 개인 브랜치 작업 → PR → dev 머지, dev 직접 push 차단, PR 메시지 형식 통일

**진행 방식**: 문제 정의·원인 분석 → 설계 → 구현 순서로 진행하고, 파트마다 협업 방식을 회고하며 개선했다 (개발 3일 + 코드 리뷰 1.5일).
- Alarm Clock: 파일·함수 단위로 나눠 병렬 구현 → 서로의 코드를 비교할 수 있었지만 사전 테스트 분석과 설계가 부족했음
- Priority: 전체 테스트를 함께 분석하고 설계·구현 범위를 문서화 → 선점과 기부의 연관성을 함께 이해하고 전체 구조를 공유
- MLFQS: 테스트를 나눠 분석하고 플로우차트로 공동 설계 → 구현 흐름이 공유되어 담당 파트가 비슷한 시점에 통과. 공통 코드 중복과 담당 범위 불명확은 다음 과제로 남김

**협업 도구·규칙**: 개인 브랜치 → PR → dev 머지, dev 직접 push 차단, PR 메시지 형식 통일, GitHub 코멘트로 코드 리뷰, GitHub Projects로 진척도 관리, Slack으로 일정·설계 문서 공유


## Project 2: User Programs

**내 역할**
- 인자 전달, 유저 포인터 검증
- fd 테이블(배열 → 연결 리스트 전환), create · remove · open · read · write · close
- fork, rox(실행 중 파일 쓰기 금지), wait / exit 통합

**트러블슈팅: multi-oom 커널 PANIC 2건**
- fork 실패 시 자식이 부모의 수거 전에 사라지던 문제 → printf 추적으로 원인 확인 후, 실패해도 부모가 수거할 때까지 대기하도록 수정
- 간헐적 TIMEOUT → 예외 시점 `rsp`가 유저 스택 영역이라는 단서로 `syscall-entry.S`를 분석해 `popq %rsp`와 `sysretq` 사이 인터럽트 틈을 발견, 핸들러 종료 후 `cli` 추가로 해결

**진행 방식**: 기능마다 테스트 분석·설계 1시간 + 구현 1시간 페어 세션. 기능 단위로 PR 5개를 나눠 단계적으로 통합했다 (① Argument Passing + write/exit/halt → ② 유저 포인터 검증 → ③ 파일 시스템 콜·fd 테이블 → ④ wait/fork/exec → ⑤ rox·multi-oom 정리)

-----

# 📘 Docker기반 Pintos 개발 환경 구축 가이드 

이 문서는 **Windows**와 **macOS** 사용자가 Docker와 VSCode DevContainer 기능을 활용하여 Pintos OS 프로젝트를 빠르게 구축할 수 있도록 도와줍니다.

[**주의**]
* ubunbu:22.04 버전은 충분한 테스트와 검증이 되지 않았습니다. 이 점을 주의해서 사용하시기 바랍니다.

[**참고**] 
* pintos 도커 환경은 `64비트 기반 X86-64` 기반의 `ubuntu:22.04` 버전을 사용합니다.
   * kaist-pintos는 오리지널 pintos와 달리 64비트 환경을 지원합니다.
   * 이번 도커 환경은 ubuntu 22.04를 지원하여 vscode의 최신 버전에서 원격 연결이 안되는 문제를 해결하였습니다.
* pintos 도커 환경은 kaist-pintos에서 추천하는 qemu 에뮬레이터를 설치하고 사용합니다. 
* pintos 도커 환경은 9주차부터 13주차까지 같은 환경을 사용합니다. 이 기간동안 별도의 개발 환경을 제공하지 않습니다.
* 기존 도커 환경과 달리 `vscode`와 통합된 디버깅 환경(F5로 시작하는)을 제공하지 않습니다. 디버깅이 필요한 경우 `gdb`를 사용하세요. 
* vscode에서 터미널을 오픈하면 자동으로 `source /workspaces/pintos_22.04_lab_docker/pintos/activate`를 실행합니다.

---

## 1. Docker란 무엇인가요?

**Docker**는 애플리케이션을 어떤 컴퓨터에서든 **동일한 환경에서 실행**할 수 있게 도와주는 **가상화 플랫폼**입니다.  

Docker는 다음 구성요소로 이루어져 있습니다:

- **Docker Engine**: 컨테이너를 실행하는 핵심 서비스
- **Docker Image**: 컨테이너 생성에 사용되는 템플릿 (레시피 📃)
- **Docker Container**: 이미지를 기반으로 생성된 실제 실행 환경 (요리 🍜)

### ✅ AWS EC2와의 차이점

| 구분 | EC2 같은 VM | Docker 컨테이너 |
|------|-------------|-----------------|
| 실행 단위 | OS 포함 전체 | 애플리케이션 단위 |
| 실행 속도 | 느림 (수십 초 이상) | 매우 빠름 (거의 즉시) |
| 리소스 사용 | 무거움 | 가벼움 |

---

## 2. VSCode DevContainer란 무엇인가요?

**DevContainer**는 VSCode에서 Docker 컨테이너를 **개발 환경**처럼 사용할 수 있게 해주는 기능입니다.

- 코드를 실행하거나 디버깅할 때 **컨테이너 내부 환경에서 동작**
- 팀원 간 **환경 차이 없이 동일한 개발 환경 구성** 가능
- `.devcontainer` 폴더에 정의된 설정을 VSCode가 읽어 자동 구성

---

## 3. Docker Desktop 설치하기

1. Docker 공식 사이트에서 설치 파일 다운로드:  
   👉 [https://www.docker.com/products/docker-desktop](https://www.docker.com/products/docker-desktop)

2. 설치 후 Docker Desktop 실행  
   - Windows: Docker 아이콘이 트레이에 떠야 함  
   - macOS: 상단 메뉴바에 Docker 아이콘 확인

---

## 4. 프로젝트 파일 다운로드 (히스토리 없이)

터미널(CMD, PowerShell, zsh 등)에서 아래 명령어로 프로젝트 폴더만 내려받습니다:

```bash
git clone --depth=1 https://github.com/krafton-jungle/pintos_22.04_lab_docker.git 
```

- `--depth=1` 옵션은 git commit 히스토리를 생략하고 **최신 파일만 가져옵니다.**

### 📂 다운로드 후 폴더 구조 설명

```
pintos_22.04_lab_docker/
├── .devcontainer/
│   ├── devcontainer.json      # VSCode에서 컨테이너 환경 설정
│   └── Dockerfile             # pintos 개발 환경 도커 이미지 정의
│
├── pintos
│   ├── threads                # 9주차 threads 프로젝트 폴더
│   ├── userprog               # 10-11주차 user program 프로젝트 폴더
│   └── vm                     # 12-13주차 virtual memory 프로젝트 폴더
│
└── README.md                  # 현재 문서
```
---

## 5. VSCode에서 해당 프로젝트 폴더 열기

1. VSCode를 실행
2. `파일 → 폴더 열기`로 방금 클론한 `pintos_22.04_lab_docker` 폴더를 선택

---

## 6. 개발 컨테이너: 컨테이너에서 열기

1. VSCode에서 `Ctrl+Shift+P` (Windows/Linux) 또는 `Cmd+Shift+P` (macOS)를 누릅니다.
2. 명령어 팔레트에서 `Dev Containers: Reopen in Container`를 선택합니다.
3. 이후 컨테이너가 자동으로 실행되고 빌드됩니다. 처음 컨테이너를 열면 빌드하는 시간이 오래걸릴 수 있습니다. 빌드 후, 프로젝트가 **컨테이너 안에서 실행됨**.

---

## 7. C 파일에 브레이크포인트 설정 후 디버깅 (F5)
pintos 랩에서는 vscode기반의 디버깅을 지원하지 않습니다. 

---
## 8. 새로운 Git 리포지토리에 Commit & Push 하기

금주 프로젝트를 개인 Git 리포와 같은 다른 리포지토리에 업로드하려면, 기존 Git 연결을 제거하고 새롭게 초기화해야 합니다.

### ✅ 완전히 새로운 Git 리포로 업로드하는 방법

아래 명령어를 순서대로 실행하세요:

```bash
rm -rf .git
git init
git remote add origin https://github.com/myusername/my-new-repo.git
git add .
git commit -m "Clean start"
git push -u origin main
```

### 📌 설명

- `rm -rf .git`: 기존 Git 기록과 연결을 완전히 삭제합니다.
- `git init`: 현재 폴더를 새로운 Git 리포지토리로 초기화합니다.
- `git remote add origin ...`: 새로운 리포지토리 주소를 origin으로 등록합니다.
- `git add .` 및 `git commit`: 모든 파일을 커밋합니다.
- `git push`: 새로운 리포에 최초 업로드(Push)합니다.

이 과정을 거치면 기존 리포와의 연결은 완전히 제거되고, **새로운 독립적인 프로젝트로 관리**할 수 있습니다.
