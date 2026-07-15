let
  host = {
    ip_addr4 = "145.220.6.20";
    ip_addr6 = "2001:67c:6ec:abba:145:220:6:20";
    hostName = "kassa2";
  };
in
{
  _module.args = { inherit host; };
  imports = [
    # keep-sorted start
    ../../base/base.nix
    ../../base/users.nix
    ../../networking/firstInterface.nix
    ../../networking/ssh.nix
    # keep-sorted end
  ];
}
