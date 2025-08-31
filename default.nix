{
  APPNAME,
  self,
}: final: prev: let
  packageOverrides = luaself: luaprev: {
    ${APPNAME} = luaself.callPackage ({buildLuarocksPackage}:
      buildLuarocksPackage {
        pname = APPNAME;
        version = "scm-1";
        knownRockspec = "${self}/${APPNAME}-scm-1.rockspec";
        src = self;
      }) {};
  };
  lua5_1 = prev.lua5_1.override {
    inherit packageOverrides;
  };
  lua51Packages = final.lua5_1.pkgs;
  lua5_2 = prev.lua5_2.override {
    inherit packageOverrides;
  };
  lua52Packages = final.lua5_2.pkgs;
  lua5_3 = prev.lua5_3.override {
    inherit packageOverrides;
  };
  lua53Packages = final.lua5_3.pkgs;
  lua5_4 = prev.lua5_4.override {
    inherit packageOverrides;
  };
  lua54Packages = final.lua5_4.pkgs;
  luajit = prev.luajit.override {
    inherit packageOverrides;
  };
  luajitPackages = final.luajit.pkgs;
  lua = prev.lua.override {
    inherit packageOverrides;
  };
  luaPackages = final.lua.pkgs;

  vimPlugins =
    prev.vimPlugins
    // {
      ${APPNAME} = final.neovimUtils.buildNeovimPlugin {
        pname = APPNAME;
        version = "dev";
        src = self;
      };
    };
in {
  inherit
    vimPlugins
    lua
    luaPackages
    luajit
    luajitPackages
    lua5_1
    lua51Packages
    lua5_2
    lua52Packages
    lua5_3
    lua53Packages
    lua5_4
    lua54Packages
    ;
}
