# Overlay the detected VS2022 compiler. Keep the working library configuration.
[settings]
build_type=RelWithDebInfo

[conf]
tools.cmake.cmaketoolchain:generator=Ninja
tools.build:jobs=4
libwebm/*:tools.build:cxxflags=["/permissive-"]
ffmpeg/*:tools.gnu:extra_configure_args=["--env=PKG_CONFIG_PATH=conan"]
openssl/*:user.openssl:windows_use_jom=True

[tool_requires]
# Build-time only; Conan verifies the official Qt archive's SHA256.
openssl/*: jom/1.1.4
