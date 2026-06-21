#include <qbuem_json/qbuem_json.hpp>
#include <cstdio>

int main() {
    auto canon = qbuem::canonicalize(std::string_view(R"({"b":1,"a":2})"));
    std::printf("canonicalize -> %s\n", canon.c_str());
    return canon == "{\"a\":2,\"b\":1}" ? 0 : 1;
}
