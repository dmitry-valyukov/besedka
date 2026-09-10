#include <gtest/gtest.h>

import wxl.core;

namespace {

/// Дерево XML настроек живёт в пуле STA, а пул строится раз на процесс и на
/// своём потоке. Глобальная обвязка gtest -- ровно то место, где это
/// делается: до первого теста и на всё время до последнего.
class sta_memory_pool_environment : public ::testing::Environment {
public:
    void SetUp() override { pool_ = std::make_unique<wxl::core::sta_memory_pool>(); }
    void TearDown() override { pool_.reset(); }

private:
    std::unique_ptr<wxl::core::sta_memory_pool> pool_;
};

::testing::Environment* const g_sta_memory_pool =
    ::testing::AddGlobalTestEnvironment(new sta_memory_pool_environment);

}  // namespace
