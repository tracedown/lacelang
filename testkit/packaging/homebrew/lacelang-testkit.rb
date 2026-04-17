# Homebrew formula for lacelang-testkit.
# brew tap lacelang/lacelang
# brew install lacelang-testkit

class LacelangTestkit < Formula
  desc "Canonical conformance test harness for Lace executors"
  homepage "https://github.com/lacelang/lacelang"
  url "https://github.com/lacelang/lacelang/archive/refs/tags/testkit-v0.1.0.tar.gz"
  sha256 "UPDATE_ON_RELEASE"
  license "MIT"
  head "https://github.com/lacelang/lacelang.git", branch: "main"

  depends_on "openssl@3"
  depends_on "python@3.12" => :build

  def install
    cd "testkit" do
      cd "src" do
        system "make", "EMBED_VECTORS=1", "WITH_TLS=1",
               "CFLAGS=-std=c99 -Wall -Wextra -Wpedantic -O2 -D_POSIX_C_SOURCE=200809L -D_GNU_SOURCE -I#{Formula["openssl@3"].opt_include} -Ithird_party/cjson",
               "LDFLAGS=-L#{Formula["openssl@3"].opt_lib} -lpthread -lssl -lcrypto"
        bin.install "build/lace-conformance"
      end
      # Ship the cert generator + its target dir. The post_install hook
      # populates the dir on install; users can re-run it later via
      # `lace-conformance --generate-certs`.
      (pkgshare/"certs").mkpath
      pkgshare.install "certs/generate-certs.sh"
    end
  end

  def post_install
    # Generate TLS test certs so the HTTPS mock is usable immediately. If
    # the system openssl CLI is absent for any reason, the script is a
    # no-op with a warning and install proceeds.
    system pkgshare/"generate-certs.sh", pkgshare/"certs"
  rescue => e
    opoo "lacelang-testkit: TLS cert generation skipped (#{e.message})."
    opoo "HTTPS mock vectors will be reported as skipped. Run `lace-conformance --generate-certs` to retry."
  end

  test do
    assert_match "lace-conformance", shell_output("#{bin}/lace-conformance --version")
    assert_match "Usage",            shell_output("#{bin}/lace-conformance --help")
  end
end
