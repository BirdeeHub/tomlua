{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  outputs = {self, nixpkgs}: let
    forAllSys = nixpkgs.lib.genAttrs nixpkgs.lib.platforms.all;
    APPNAME = "tomlua";
  in {
    overlays.default = final: prev: let
      args = {
        packageOverrides = luaself: luaprev: {
          ${APPNAME} = luaself.callPackage ({buildLuarocksPackage}:
            buildLuarocksPackage {
              pname = APPNAME;
              version = "scm-1";
              knownRockspec = "${self}/${APPNAME}-scm-1.rockspec";
              src = self;
            }) {};
        };
      };
    in {
      lua5_1 = prev.lua5_1.override args;
      lua51Packages = final.lua5_1.pkgs;
      lua5_2 = prev.lua5_2.override args;
      lua52Packages = final.lua5_2.pkgs;
      lua5_3 = prev.lua5_3.override args;
      lua53Packages = final.lua5_3.pkgs;
      lua5_4 = prev.lua5_4.override args;
      lua54Packages = final.lua5_4.pkgs;
      luajit = prev.luajit.override args;
      luajitPackages = final.luajit.pkgs;
      lua = prev.lua.override args;
      luaPackages = final.lua.pkgs;
      vimPlugins = prev.vimPlugins // {
        ${APPNAME} = final.neovimUtils.buildNeovimPlugin {
          pname = APPNAME;
          version = "dev";
          src = self;
        };
      };
    };
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
