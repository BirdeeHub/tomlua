{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  outputs = {self, nixpkgs}: let
    forAllSys = nixpkgs.lib.genAttrs nixpkgs.lib.platforms.all;
    APPNAME = "tomlua";
  in {
    overlays.default = import ./overlay.nix { inherit APPNAME self; };
    packages = forAllSys (system: let
      pkgs = import nixpkgs { inherit system; overlays = [ self.overlays.default ]; };
    in {
      default = pkgs.vimPlugins.${APPNAME};
      ${APPNAME} = self.outputs.default;
    });
    devShells = forAllSys (system: let
      pkgs = import nixpkgs { inherit system; overlays = [ self.overlays.default ]; };
      lua = pkgs.luajit.withPackages (lp: [lp.inspect lp.cjson lp.toml-edit]);
    in {
      default = pkgs.mkShell {
        name = "${APPNAME}-dev";
        packages = [ lua pkgs.luarocks ];
        LUA_INCDIR = "${lua}/include";
        LUA = lua.interpreter;
        GREP = "${pkgs.gnugrep}/bin/grep";
        BEAR = "${pkgs.bear}/bin/bear";
        shellHook = ''
          make clean build bear
          [ "$(whoami)" == "birdee" ] && exec zsh
        '';
      };
    });
  };
}
