#include <gtest/gtest.h>

import besedka.rsdn;

namespace {

/// Everything the family allocates comes from the STA pool, so one has to
/// exist before the first test and outlive the last. It may be constructed
/// only once per process, which is exactly what a global environment gives.
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
