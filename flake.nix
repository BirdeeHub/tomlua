{
  inputs.nixpkgs.url = "github:nixos/nixpkgs/nixpkgs-unstable";
  outputs = { self, nixpkgs, ... }@inputs: let
    genAttrs = names: f: builtins.listToAttrs (map (n: { name = n; value = f n; }) names);
    forAllSys = genAttrs inputs.systems or nixpkgs.lib.platforms.all or [ inputs.system or builtins.throw "No systems list provided!" ];
    getpkgswithoverlay = system: if builtins.all (v: nixpkgs ? "${v}") [ "path" "system" "appendOverlays" ]
      then nixpkgs.appendOverlays [ overlay ] else import nixpkgs { inherit system; overlays = [ overlay ]; };
    getpkgs = system: if builtins.all (v: nixpkgs ? "${v}") [ "path" "system" "appendOverlays" ]
      then nixpkgs else import nixpkgs { inherit system; };
    APPNAME = "tomlua";
    l_pkg_enum = {
      lua5_1 = "lua51Packages";
      lua5_2 = "lua52Packages";
      lua5_3 = "lua53Packages";
      lua5_4 = "lua54Packages";
      luajit = "luajitPackages";
      lua = "luaPackages";
    };
    overlay = final: prev: let
      # lua5_1 = prev.lua5_1.override { packageOverrides };
      l_pkg_main = builtins.mapAttrs (n: _: (prev.lib.attrByPath [ n "override" ] null prev) {
        packageOverrides = luaself: luaprev: {
          ${APPNAME} = luaself.callPackage ({buildLuarocksPackage}:
            buildLuarocksPackage {
              pname = APPNAME;
              version = "scm-1";
              knownRockspec = "${self}/${APPNAME}-scm-1.rockspec";
              src = self;
              installCheckPhase = ''
                runHook preInstallCheck
                luarocks test
                runHook postInstallCheck
              '';
            }) {};
        };
      }) l_pkg_enum;
      # lua51Packages = final.lua5_1.pkgs;
      l_pkg_sets = builtins.listToAttrs (prev.lib.mapAttrsToList (n: v: {
        name = v;
        value = prev.lib.attrByPath [ n "pkgs" ] null final;
      }) l_pkg_enum);
    in l_pkg_main // l_pkg_sets // {
      vimPlugins = prev.vimPlugins // {
        ${APPNAME} = (final.neovimUtils.buildNeovimPlugin { pname = APPNAME; }).overrideAttrs {
          luarocksConfig = {
            lua_modules_path = "lua";
            lib_modules_path = "lua";
          };
        };
      };
    };
    packages = forAllSys (system: let
      pkgs = getpkgswithoverlay system;
    in (with builtins; pkgs.lib.pipe l_pkg_enum [
      attrNames
      (map (n: {
        name = "${n}-${APPNAME}";
        value = pkgs.lib.attrByPath [ n "pkgs" APPNAME ] null pkgs;
      }))
      listToAttrs
    ]) // {
      default = pkgs.vimPlugins.${APPNAME};
      "vimPlugins-${APPNAME}" = pkgs.vimPlugins.${APPNAME};
    });
  in {
    overlays.default = overlay;
    inherit packages;
    checks = forAllSys (system: builtins.mapAttrs (_: p: p.overrideAttrs { doInstallCheck = true; }) packages.${system});
    devShells = forAllSys (system: let
      pkgs = getpkgs system;
      lua = pkgs.luajit.withPackages (lp: [lp.inspect lp.cjson lp.toml-edit lp.luarocks]);
    in {
      default = pkgs.mkShell {
        name = "${APPNAME}-dev";
        packages = [ lua ];
        LUA_INCDIR = "${lua}/include";
        LUA = lua.interpreter;
        GREP = "${pkgs.gnugrep}/bin/grep";
        BEAR = "${pkgs.bear}/bin/bear";
        shellHook = ''
          make clean bear build
          [ "$(whoami)" == "birdee" ] && exec zsh
        '';
      };
    });
  };
}
