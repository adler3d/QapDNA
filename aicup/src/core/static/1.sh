echo curl -X POST "http://localhost:12346/whitelist" \
             -H "Authorization: Bearer s3cr3t_t0k3n_f0r_cdn_uploads_2025" \
             -d "ips=192.168.1.100,192.168.1.101,10.0.0.5&action=add"
curl -X PUT -T universal-runner.tar \
  -H 'Authorization: Bearer s3cr3t_t0k3n_f0r_cdn_uploads_2025' \
  http://localhost:12346/images/universal-runner.tar