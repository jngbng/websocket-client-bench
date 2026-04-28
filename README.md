# Simple C++ websocket client performance benchmark

간단한 C++ 웹소켓 클라이언트 성능측정이다.

libwebsocket의 benchmark를 참고했다. 샘플 웹소켓 서버를 두고, 해당 서버에 메시지를 보내고 echo 응답을 받는 횟수를 측정했다.

일부 라이브러리는 async runtime, event loop 구현체들을 포함한다.

## TL;DR; 결과

CPU boost ON, RPS pinning OFF

500 connections, no-gzip, payload: 1024 bytes (pick 2nd result):

| library          | compile-time | speed (msg/sec) | KiB/sec |
| ---------------- | ------------ | --------------- | ------- |
| uwebsocket(max)  | 0.826s       | 109,653         | 110,081 |
| wspp             | 4.385s       | 81,200          | 81,200  |
| boost/beast      | 13.981s      | 56,700          | 56,700  |
| libwebsocket     | 3.956s       | 49,887          | 49,887  |
| libhv            | 5.357s       | 86,670          | 86,670  |


2 connections, no-gzip, payload: 512 bytes (pick 2nd result):

| library          | compile-time | speed (msg/sec) | Kib/sec |
| ---------------- | ------------ | --------------- | ------- |
| uwebsocket(max)  | 0.826s       | 102,090         | 51,443  |
| wspp             | 4.385s       | 98,050          | 49,025  |
| boost/beast      | 13.981s      | 63,035          | 31,517  |
| libwebsocket     | 3.956s       | 61,799          | 30,899  |
| libhv            | 5.357s       | 63,502          | 31,751  |


CPU boost OFF, RPS pinning ON

500 connections, no-gzip, payload: 1024 bytes (pick 2nd result):

| library          | compile-time | speed (msg/sec) | KiB/sec |
| ---------------- | ------------ | --------------- | ------- |
| uwebsocket(max)  | 0.826s       | 52,311          | 52,515  |
| wspp             | 4.385s       | 60,408          | 60,408  |
| boost/beast      | 13.981s      | 45,050          | 45,050  |
| libwebsocket     | 3.956s       | 42,027          | 42,027  |
| libhv            | 5.357s       | 56,427          | 56,427  |


2 connections, no-gzip, payload: 512 bytes (pick 2nd result):

| library          | compile-time | speed (msg/sec) | Kib/sec |
| ---------------- | ------------ | --------------- | ------- |
| uwebsocket(max)  | 0.826s       | 31,204          | 15,723  |
| wspp             | 4.385s       | 36,497          | 18,248  |
| boost/beast      | 13.981s      | 28,295          | 14,147  |
| libwebsocket     | 3.956s       | 22,837          | 11,418  |
| libhv            | 5.357s       | 28,205          | 14,102  |

Hardware:
```
OS: Linux fedora 6.19.11-200.fc43.x86_64 #1 SMP PREEMPT_DYNAMIC Thu Apr  2 16:55:52 UTC 2026 x86_64 GNU/Linux
Comiler: gcc (GCC) 15.2.1 20260123 (Red Hat 15.2.1-7)
CPU: AMD Ryzen 5 5560U with Radeon Graphics
```

### 결과 분석

- `uwebsocket(max)`(v20.71.0): 실제 웹소켓 클라이언트 구현체는 아니다. tcp socket을 이용해 웹소켓 프로토콜을 시뮬레이션한다. 해당 시스템에서 나올 수 있는 웹소켓 클라이언트의 최대 성능을 측정하기 위한 기준이 된다.
- [wspp](https://github.com/pinwhell/wspp)(v0.1.0): 라이브러리가 event loop를 제공하진 않기 때문에, N개의 연결을 시뮬레이션하기 위해 busy polling 방식으로 구현했다. gzip compress 기능이 없다.
- `boost/beast`(v1.90.0): C++ 애증의 프로젝트 Boost에 포함된 구현체. 컴파일 속도가 너무 느리고, 사용이 편하진 않다.
- `libwebsocket`(v4.3.5): 중국산 C구현체. system event loop를 사용했다. 생각보단 느렸다. 다른 구현체와 다르게 매번 send 버퍼에 메시지를 복사하는 동작이 들어가서 그런가 싶기도 하다.
- [`CAF`](https://github.com/actor-framework/actor-framework): C++ actor 구현체. 웹소켓 구현체가 experimental로 제공된다.
- [`PhotonLibOS`](https://github.com/alibaba/PhotonLibOS): v0.9.4 (on-dev) 에서 간단한 웹소켓 구현체가 추가되었다.


`libwebsocket`와 `boost/beast`의 성능이 비슷한게 신기했다. `libwebsocket`이 더 빠르길 기대했는데, 오히려 살짝 느리게 나왔다.

`wspp`는 왜 훨씬 빠른지 궁금하다. 다른 구현체는 자체 event loop 기능을 들어가는데, wspp는 epoll을 쓰지만, 여러 connection을 묶어서 polling하는 API가 없어서 busy-polling으로 구현해버렸기 때문인가 싶기도 하다. 혹은 일부 기능이 빠져 있나? 자동 heartbeat이라던가. `uwebsocket`이 client 구현체를 제공 안 하는게 아쉬운데, `usockets` + `wspp`로 만들면 어떨까?

## TODO

- 더 엄밀한 성능 측정 방법 고려
- 다른 라이브러리 추가:
  - https://github.com/actor-framework/actor-framework (experimental)
  - https://github.com/alibaba/PhotonLibOS (0.9.4 에서 포함될 예정)
  - https://github.com/ithewei/libhv
  - https://github.com/tatsuhiro-t/wslay + I/O lib
  - https://github.com/Qihoo360/evpp + websocket lib
  - https://github.com/cesanta/mongoose


## 측정 오차 줄이기

https://it-jinsu.tistory.com/3

```
$ sudo dnf install -y stress
```

CPU boost 일시적으로 끄기
```
$ echo "0" | sudo tee /sys/devices/system/cpu/cpufreq/boost
```

[RPS](https://docs.redhat.com/en/documentation/red_hat_enterprise_linux/6/html/performance_tuning_guide/network-rps)를 써서 loopback 패킷 처리 CPU 피닝: (효과는 없는 듯?)
```
$ echo "1" | sudo tee /sys/class/net/lo/queues/rx-0/rps_cpus
```

적용되어 있는지 확인:
```
$ stess -c 12

다른 탭에서 clock이 고정되어 있는 지 확인
$ watch -n1 "grep MHz /proc/cpuinfo"
```

설정 되돌리기
```
$ echo "1" | sudo tee /sys/devices/system/cpu/cpufreq/boost
$ echo "000" | sudo tee /sys/class/net/lo/queues/rx-0/rps_cpus
```

## Memo

Run server:
```
$ taskset -c 1 ./build/uwebsockets/ws_echo_server
```

Run client:
```
$ taskset -c 3 ./build/uwebsockets/load_test_websocket 500 127.0.0.1 9001 0 0 1024
$ taskset -c 3 ./build/wspp/wspp_client --connections 500 --payload 1024
$ taskset -c 3 ./build/boost_beast/beast_client --connections 500 --payload 1024
$ taskset -c 3 ./build/libwebsockets/lws_client --connections 500 --payload 1024
$ taskset -c 3 ./build/libhv/hv_client --connections 500 --payload 1024

$ taskset -c 3 ./build/uwebsockets/load_test_websocket 2 127.0.0.1 9001 0 0 512
$ taskset -c 3 ./build/wspp/wspp_client --connections 2 --payload 512
$ taskset -c 3 ./build/boost_beast/beast_client --connections 2 --payload 512
$ taskset -c 3 ./build/libwebsockets/lws_client --connections 2 --payload 512
$ taskset -c 3 ./build/libhv/hv_client --connections 2 --payload 512
```


## Reference

- https://github.com/fffaraz/awesome-cpp
- https://github.com/facundofarias/awesome-websockets
