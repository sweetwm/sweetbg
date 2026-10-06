{
  lib,
  stdenv,
  meson,
  ninja,
  pkg-config,
  scdoc,
  wayland-scanner,
  versionCheckHook,
  wayland,
  wayland-protocols,
  libpng,
  libjpeg_turbo,
  libwebp,
}:
stdenv.mkDerivation {
  pname = "sweetbg";
  version = builtins.head (
    builtins.match ".*version: '([0-9.]+)'.*" (builtins.readFile ../meson.build)
  );

  # Only what the build needs
  src = lib.fileset.toSource {
    root = ../.;
    fileset = lib.fileset.unions [
      ../meson.build
      ../meson_options.txt
      ../src
      ../protocol
      ../doc
      ../systemd
    ];
  };

  strictDeps = true;
  depsBuildBuild = [ pkg-config ];

  nativeBuildInputs = [
    meson
    ninja
    pkg-config
    scdoc
    wayland-scanner
  ];

  buildInputs = [
    wayland
    wayland-protocols
    libpng
    libjpeg_turbo
    libwebp
  ];

  mesonFlags = [ "-Dsystemd=disabled" ];

  postInstall = ''
    install -Dm644 ../systemd/sweetbgd.service -t $out/lib/systemd/user
    substituteInPlace $out/lib/systemd/user/sweetbgd.service \
      --replace-fail "ExecStart=sweetbgd" "ExecStart=$out/bin/sweetbgd"
  '';

  nativeInstallCheckInputs = [ versionCheckHook ];
  doInstallCheck = true;

  meta = {
    description = "Small, lightweight static Wayland wallpaper daemon";
    homepage = "https://github.com/sweetwm/sweetbg";
    license = lib.licenses.gpl3Plus;
    mainProgram = "sweetbg";
    platforms = lib.platforms.linux;
  };
}
