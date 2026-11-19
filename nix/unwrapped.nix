{
  lib,
  stdenv,
  cmake,
  cmark,
  extra-cmake-modules,
  gamemode,
  jdk17,
  kdePackages,
  libnbtplusplus,
  ninja,
  onnxruntime,
  qrencode,
  self,
  stripJavaArchivesHook,
  symlinkJoin,
  tomlplusplus,
  zlib,
  msaClientID ? null,
  libarchive,
}:

let
  date =
    let
      # YYYYMMDD
      date' = lib.substring 0 8 self.lastModifiedDate;
      year = lib.substring 0 4 date';
      month = lib.substring 4 2 date';
      date = lib.substring 6 2 date';
    in
    if (self ? "lastModifiedDate") then
      lib.concatStringsSep "-" [
        year
        month
        date
      ]
    else
      "unknown";

  # onnxruntime is split into outputs: the shared library lives in $out while
  # the headers only exist in $dev. The launcher's CMake expects a single
  # onnxruntime_ROOT containing both, so glue the outputs together.
  onnxruntimeRoot = symlinkJoin {
    name = "onnxruntime-root";
    paths = [
      onnxruntime.out
      onnxruntime.dev
    ];
  };
in

stdenv.mkDerivation {
  pname = "pollymc-unwrapped";
  version = "10.0-unstable-${date}";

  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../CMakeLists.txt
      ../CHANGELOG.md
      ../COPYING.md

      ../buildconfig
      ../cmake
      ../launcher
      ../libraries
      ../program_info
      ../tests
    ];
  };

  postUnpack = ''
    rm -rf source/libraries/libnbtplusplus
    ln -s ${libnbtplusplus} source/libraries/libnbtplusplus
  '';

  nativeBuildInputs = [
    cmake
    ninja
    extra-cmake-modules
    jdk17
    stripJavaArchivesHook
  ];

  buildInputs = [
    cmark
    kdePackages.qtbase
    kdePackages.qtnetworkauth
    kdePackages.qtsvg
    kdePackages.qtwebsockets
    onnxruntime
    qrencode
    libarchive
    tomlplusplus
    zlib
  ]
  ++ lib.optional stdenv.hostPlatform.isLinux gamemode;

  cmakeFlags = [
    # downstream branding
    (lib.cmakeFeature "Launcher_BUILD_PLATFORM" "nixpkgs")
    # headers from $dev and the library from $out, under one root
    (lib.cmakeFeature "onnxruntime_ROOT" "${onnxruntimeRoot}")
  ]
  ++ lib.optionals (msaClientID != null) [
    (lib.cmakeFeature "Launcher_MSA_CLIENT_ID" (toString msaClientID))
  ]
  ++ lib.optionals stdenv.hostPlatform.isDarwin [
    # we wrap our binary manually
    (lib.cmakeFeature "INSTALL_BUNDLE" "nodeps")
    # disable built-in updater
    (lib.cmakeFeature "MACOSX_SPARKLE_UPDATE_FEED_URL" "''")
    (lib.cmakeFeature "CMAKE_INSTALL_PREFIX" "${placeholder "out"}/Applications/")
  ];

  doCheck = true;

  dontWrapQtApps = true;

  meta = {
    description = "Free, open source launcher for Minecraft";
    longDescription = ''
      Allows you to have multiple, separate instances of Minecraft (each with
      their own mods, texture packs, saves, etc) and helps you manage them and
      their associated options with a simple interface.
    '';
    homepage = "https://pollymc.vercel.app";
    license = lib.licenses.gpl3Only;
    maintainers = with lib.maintainers; [
      Scrumplex
      getchoo
    ];
    mainProgram = "pollymc";
    platforms = lib.platforms.linux ++ lib.platforms.darwin;
  };
}
