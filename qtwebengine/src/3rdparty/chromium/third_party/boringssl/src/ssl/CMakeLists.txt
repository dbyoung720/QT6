add_library(
  ssl

  bio_ssl.cc
  d1_both.cc
  d1_lib.cc
  d1_pkt.cc
  d1_srtp.cc
  dtls_method.cc
  dtls_record.cc
  encrypted_client_hello.cc
  extensions.cc
  handoff.cc
  handshake.cc
  handshake_client.cc
  handshake_server.cc
  s3_both.cc
  s3_lib.cc
  s3_pkt.cc
  ssl_aead_ctx.cc
  ssl_asn1.cc
  ssl_buffer.cc
  ssl_cert.cc
  ssl_cipher.cc
  ssl_file.cc
  ssl_key_share.cc
  ssl_lib.cc
  ssl_privkey.cc
  ssl_session.cc
  ssl_stat.cc
  ssl_transcript.cc
  ssl_versions.cc
  ssl_x509.cc
  t1_enc.cc
  tls_method.cc
  tls_record.cc
  tls13_both.cc
  tls13_client.cc
  tls13_enc.cc
  tls13_server.cc
)
# Although libssl also provides headers that require an include directory, the
# flag is already specified by libcrypto, so we omit target_include_directories
# here.
install_if_enabled(TARGETS ssl EXPORT OpenSSLTargets ${INSTALL_DESTINATION_DEFAULT})
set_property(TARGET ssl PROPERTY EXPORT_NAME SSL)
target_link_libraries(ssl crypto)

add_executable(
  ssl_test

  span_test.cc
  ssl_test.cc
  ssl_c_test.c

  $<TARGET_OBJECTS:boringssl_gtest_main>
)
target_link_libraries(ssl_test test_support_lib boringssl_gtest ssl crypto)
add_dependencies(all_tests ssl_test)
