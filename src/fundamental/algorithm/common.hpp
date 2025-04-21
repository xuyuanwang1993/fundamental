#pragma once
#include <algorithm>
#include <random>
#include <vector>
namespace Fundamental
{
template <typename Container,
          typename = std::enable_if_t<std::is_same_v<
              typename std::iterator_traits<decltype(std::begin(std::declval<Container>()))>::iterator_category,
              std::random_access_iterator_tag>>>
void shuffle(Container& c) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::shuffle(std::begin(c), std::end(c), g);
}

} // namespace Fundamental
