if (WITH_TLS)
    if (XMRIG_DEPS AND EXISTS "${XMRIG_DEPS}/include/openssl")
        set(OPENSSL_ROOT_DIR ${XMRIG_DEPS})
    endif()

    if (WIN32)
        set(OPENSSL_USE_STATIC_LIBS TRUE)
        set(OPENSSL_MSVC_STATIC_RT TRUE)

        set(EXTRA_LIBS ${EXTRA_LIBS} crypt32)
    elseif (APPLE)
        set(OPENSSL_USE_STATIC_LIBS TRUE)
    endif()

    if (BUILD_STATIC)
        set(OPENSSL_USE_STATIC_LIBS TRUE)
    endif()


    find_package(OpenSSL)

    if (OPENSSL_FOUND)
        set(TLS_SOURCES
            src/base/net/stratum/Tls.cpp
            src/base/net/stratum/Tls.h
            src/base/net/tls/ServerTls.cpp
            src/base/net/tls/ServerTls.h
            src/base/net/tls/TlsConfig.cpp
            src/base/net/tls/TlsConfig.h
            src/base/net/tls/TlsContext.cpp
            src/base/net/tls/TlsContext.h
            src/base/net/tls/TlsGen.cpp
            src/base/net/tls/TlsGen.h
            )

        include_directories(${OPENSSL_INCLUDE_DIR})

        if (WITH_HTTP)
            set(TLS_SOURCES ${TLS_SOURCES}
                src/base/net/https/HttpsClient.cpp
                src/base/net/https/HttpsClient.h
                src/base/net/https/HttpsContext.cpp
                src/base/net/https/HttpsContext.h
                src/base/net/https/HttpsServer.cpp
                src/base/net/https/HttpsServer.h
                src/base/net/dns/DnsPoolNsBackend.cpp
                src/base/net/dns/DnsPoolNsBackend.h
                src/base/net/dns/DnsTcpClient.cpp
                src/base/net/dns/DnsTcpClient.h
                src/base/net/dns/DnsWireFormat.cpp
                src/base/net/dns/DnsWireFormat.h
                src/base/net/dns/DomainUtils.cpp
                src/base/net/dns/DomainUtils.h
                )

            if (WITH_HTTP2 AND NGHTTP2_FOUND)
                set(TLS_SOURCES ${TLS_SOURCES}
                    src/base/net/http2/Http2Client.cpp
                    src/base/net/http2/Http2Client.h
                    )
                include_directories(${NGHTTP2_INCLUDE_DIR})
                add_definitions(/DXMRIG_FEATURE_HTTP2)
            endif()
        endif()
    else()
        message(FATAL_ERROR "OpenSSL NOT found: use `-DWITH_TLS=OFF` to build without TLS support")
    endif()

    add_definitions(/DXMRIG_FEATURE_TLS)
else()
    set(TLS_SOURCES "")
    set(OPENSSL_LIBRARIES "")
    remove_definitions(/DXMRIG_FEATURE_TLS)

    if (WITH_HTTP)
        set(TLS_SOURCES ${TLS_SOURCES}
            src/base/net/http/HttpServer.cpp
            src/base/net/http/HttpServer.h
            )
    endif()

    set(CMAKE_PROJECT_NAME "${CMAKE_PROJECT_NAME}-notls")
endif()
