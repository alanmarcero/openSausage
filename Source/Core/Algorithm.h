#pragma once

namespace sausage {
enum class Algorithm { Classic, Velvet, Punch, Prism };
inline constexpr const char* algorithmNames[] {"Classic", "Velvet", "Punch", "Prism"};
inline Algorithm validAlgorithm(Algorithm algorithm) noexcept {
    return algorithm >= Algorithm::Classic && algorithm <= Algorithm::Prism ? algorithm : Algorithm::Classic;
}
}
