{
  services.openssh = {
    enable = true;
  };
  services.fail2ban = {
    enable = true;
    bantime = "2h";
    bantime-increment.enable = true;
    maxretry = 5;
  };
}
