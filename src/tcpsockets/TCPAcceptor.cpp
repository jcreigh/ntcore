/*
   TCPAcceptor.cpp

   TCPAcceptor class definition. TCPAcceptor provides methods to passively
   establish TCP/IP connections with clients.

   ------------------------------------------

   Copyright � 2013 [Vic Hargrave - http://vichargrave.com]

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "TCPAcceptor.h"

#include <cstdio>
#include <cstring>
#ifdef _WIN32
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

#include "llvm/SmallString.h"
#include "../Log.h"
#include "SocketError.h"

using namespace tcpsockets;

TCPAcceptor::TCPAcceptor(int port, const char* address)
    : m_lsd(0),
      m_port(port),
      m_address(address),
      m_listening(false),
      m_shutdown(false) {
#ifdef _WIN32
  WSAData wsaData;
  WORD wVersionRequested = MAKEWORD(2, 2);
  WSAStartup(wVersionRequested, &wsaData);
#endif
}

TCPAcceptor::~TCPAcceptor() {
  if (m_lsd > 0) {
    shutdown();
#ifdef _WIN32
    closesocket(m_lsd);
#else
    close(m_lsd);
#endif
  }
#ifdef _WIN32
  WSACleanup();
#endif
}

int TCPAcceptor::start() {
  if (m_listening) return 0;

  m_lsd = socket(PF_INET, SOCK_STREAM, 0);
  struct sockaddr_in address;

  std::memset(&address, 0, sizeof(address));
  address.sin_family = PF_INET;
  if (m_address.size() > 0) {
#ifdef _WIN32
    llvm::SmallString<128> addr_copy(m_address);
    addr_copy.push_back('\0');
    int size = sizeof(address);
    WSAStringToAddress(addr_copy.data(), PF_INET, nullptr, (struct sockaddr*)&address, &size);
#else
    inet_pton(PF_INET, m_address.c_str(), &(address.sin_addr));
#endif
  } else {
    address.sin_addr.s_addr = INADDR_ANY;
  }
  address.sin_port = htons(m_port);

  int optval = 1;
  setsockopt(m_lsd, SOL_SOCKET, SO_REUSEADDR, (char*)&optval, sizeof optval);

  int result = bind(m_lsd, (struct sockaddr*)&address, sizeof(address));
  if (result != 0) {
    ERROR("bind() failed: " << SocketStrerror());
    return result;
  }

  result = listen(m_lsd, 5);
  if (result != 0) {
    ERROR("listen() failed: " << SocketStrerror());
    return result;
  }
  m_listening = true;
  return result;
}

void TCPAcceptor::shutdown() {
  m_shutdown = true;
#ifdef _WIN32
  ::shutdown(m_lsd, SD_BOTH);
#else
  ::shutdown(m_lsd, SHUT_RDWR);
#endif
}

std::unique_ptr<NetworkStream> TCPAcceptor::accept() {
  if (!m_listening) return nullptr;

  struct sockaddr_in address;
#ifdef _WIN32
  int len = sizeof(address);
#else
  socklen_t len = sizeof(address);
#endif
  std::memset(&address, 0, sizeof(address));
  int sd = ::accept(m_lsd, (struct sockaddr*)&address, &len);
  if (sd < 0) {
    if (!m_shutdown) ERROR("accept() failed: " << SocketStrerror());
    return nullptr;
  }
  return std::unique_ptr<NetworkStream>(new TCPStream(sd, &address));
}
