// Compile the Conan-packaged backend as an external include to suppress upstream warnings under /W4 /WX.
// NOLINTNEXTLINE(bugprone-suspicious-include): The Conan package ships this backend as a .cpp implementation.
#include <imgui_impl_vulkan.cpp>
