#!/usr/bin/env python3
"""Optional PC-side GameCube cover downloader; does not run on Wii."""
import argparse, pathlib, urllib.request, time, re, json
PNG=b"\x89PNG\r\n\x1a\n"
def ids(root):
    for p in root.rglob("*"):
        if not p.is_file() or p.suffix.lower() not in (".iso",".gcm",".ciso"):continue
        try:
            with p.open("rb") as f: code=f.read(6)
        except OSError:continue
        if re.fullmatch(rb"[A-Za-z0-9]{6}",code):yield code.decode("ascii").upper(),p
def download(url,dest):
    req=urllib.request.Request(url,headers={"User-Agent":"MAIZO-Wii-CoverSync/0.1 (+https://github.com/maizon/MaizoWii)"})
    with urllib.request.urlopen(req,timeout=12) as r:
        if r.status!=200:return False
        data=r.read(2_000_001)
    if len(data)>2_000_000 or not data.startswith(PNG):return False
    dest.write_bytes(data);return True
def main():
    p=argparse.ArgumentParser()
    p.add_argument("device",type=pathlib.Path,help="mounted USB or SD root")
    p.add_argument("--region",default="US",choices=["US","EN","FR","DE","ES","IT","JP"])
    p.add_argument("--limit",type=int,default=100)
    a=p.parse_args();out=a.device/"maizo"/"covers";out.mkdir(parents=True,exist_ok=True)
    games=a.device/"games"
    if not games.is_dir():raise SystemExit("Expected <device>/games")
    stats={"downloaded":0,"existing":0,"missing":0}
    for idx,(code,game) in enumerate(dict(ids(games)).items()):
        if idx>=a.limit:break
        dest=out/(code+".png")
        if dest.exists():stats["existing"]+=1;continue
        ok=False
        for region in (a.region,"US") if a.region!="US" else ("US",):
            url=f"https://art.gametdb.com/gc/cover/{region}/{code}.png"
            try:
                if download(url,dest):ok=True;break
            except Exception as ex:
                print(f"WARN {code} {region}: {ex}")
        stats["downloaded" if ok else "missing"]+=1
        print(f"{code}: {'OK' if ok else 'not found'}")
        time.sleep(0.15)
    print(json.dumps(stats,indent=2))
if __name__=="__main__":main()
