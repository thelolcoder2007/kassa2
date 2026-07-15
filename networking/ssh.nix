{
  services.openssh = {
    enable = true;
  };
  services.fail2ban = {
    enable = true;
    bantime = "1h";
    bantime-increment.enable = true;
    maxretry = 10;
  };
}
