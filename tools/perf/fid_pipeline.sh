#!/usr/bin/env bash
# fid_pipeline.sh <outfile> — wait for fidelity sweep end, fetch, verify.
IP=192.168.4.52
OUT=$1
T0=$(date +%s)
while curl -s --max-time 2 -o /dev/null "http://$IP:8080/"; do
  [ $(( $(date +%s)-T0 )) -gt 90 ] && break
  sleep 2
done
while ! curl -s --max-time 2 -o /dev/null "http://$IP:8080/"; do
  [ $(( $(date +%s)-T0 )) -gt 4500 ] && { echo "FID SWEEP TIMEOUT"; exit 1; }
  sleep 10
done
sleep 5
for i in 1 2 3 4 5; do
  if curl -s --max-time 30 "http://$IP:8080/forsaken_fidelity.txt" -o "$OUT" 2>/dev/null \
     && [ "$(grep -c '^FID: level=' "$OUT")" -ge 28 ]; then
    echo "FID SWEEP OK lines=$(grep -c '^FID: level=' "$OUT") md5=$(md5sum "$OUT" | cut -d' ' -f1)"
    exit 0
  fi
  sleep 5
done
echo "FID SWEEP VERIFY FAILED"
exit 1
