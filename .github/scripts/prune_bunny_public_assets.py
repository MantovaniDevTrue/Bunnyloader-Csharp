import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.request

REPO = "MantovaniDevTrue/Bunnyloader-Csharp"
ROOT = "https://api.github.com/repos/" + REPO
RUNTIME_FILE = "BunnyRuntime_Terraria_1.4.5.8.6.zip"


def request(path, method="GET"):
    token = os.environ.get("BUNNY_PUBLIC_CLEANUP_TOKEN", "") or os.environ.get("GH_TOKEN", "")
    headers = {
        "Accept": "application/vnd.github+json",
        "User-Agent": "BunnyLoader-Public-Binary-Cleanup",
        "X-GitHub-Api-Version": "2022-11-28",
    }
    if token:
        headers["Authorization"] = "Bearer " + token
    req = urllib.request.Request(ROOT + path, headers=headers, method=method)
    for attempt in range(4):
        try:
            with urllib.request.urlopen(req, timeout=45) as result:
                raw = result.read()
                return json.loads(raw) if raw else None
        except urllib.error.HTTPError as exc:
            if exc.code not in (429, 502, 503, 504) or attempt == 3:
                raise RuntimeError(f"GitHub API {method} {path} failed with HTTP {exc.code}") from exc
            time.sleep(2 * (attempt + 1))


def list_releases():
    results = []
    page = 1
    while True:
        batch = request(f"/releases?per_page=100&page={page}")
        if not isinstance(batch, list):
            raise RuntimeError("GitHub did not return a list of releases")
        results.extend(batch)
        if len(batch) < 100:
            break
        page += 1
    return results


def should_remove(asset):
    name = asset["name"].lower()
    return (
        name.endswith(".apk")
        or name.endswith(".apk.sha256")
        or name.endswith(".jks")
        or name == RUNTIME_FILE.lower()
    )


def run(execute):
    releases = list_releases()
    doomed = []
    empty_releases = []
    keep_assets = 0
    for release in releases:
        remove = [asset for asset in release["assets"] if should_remove(asset)]
        keep_assets += len(release["assets"]) - len(remove)
        if remove:
            doomed.extend((release["tag_name"], asset["id"], asset["name"]) for asset in remove)
            if len(remove) == len(release["assets"]):
                empty_releases.append((release["id"], release["tag_name"]))
    print(
        f"BUNNY_CLEANUP_INVENTORY releases={len(releases)} "
        f"assets_to_remove={len(doomed)} "
        f"empty_releases_after={len(empty_releases)} "
        f"retained_mods_and_sources={keep_assets}",
        flush=True,
    )
    for tag, asset_id, name in doomed[:10]:
        print(f"  candidate={tag}/{name} id={asset_id}", flush=True)
    if not execute:
        print("BUNNY_PUBLIC_CLEANUP_DRY_RUN=PASS", flush=True)
        return
    if not os.environ.get("BUNNY_PUBLIC_CLEANUP_TOKEN"):
        raise RuntimeError("BUNNY_PUBLIC_CLEANUP_TOKEN is required for deletion")
    if os.environ.get("BUNNY_PUBLIC_CLEANUP_APPROVED") != "YES":
        raise RuntimeError("Private dependency migration and signed APK build must be verified")
    for n, (tag, asset_id, name) in enumerate(doomed, start=1):
        request(f"/releases/assets/{asset_id}", "DELETE")
        if n % 25 == 0 or n == len(doomed):
            print(f"BUNNY_PUBLIC_ASSETS_DELETED={n}/{len(doomed)}", flush=True)
    for n, (release_id, tag) in enumerate(empty_releases, start=1):
        request(f"/releases/{release_id}", "DELETE")
        if n % 25 == 0 or n == len(empty_releases):
            print(f"BUNNY_EMPTY_RELEASES_REMOVED={n}/{len(empty_releases)}", flush=True)
    remaining = list_releases()
    left = [(r["tag_name"], a["name"]) for r in remaining for a in r["assets"] if should_remove(a)]
    if left:
        raise RuntimeError(f"Remaining unwanted assets: {left[:8]}")
    print("BUNNY_PUBLIC_BINARY_CLEANUP_VERIFIED=PASS", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--execute", action="store_true")
    args = parser.parse_args()
    try:
        run(args.execute)
    except Exception as exc:
        print(f"BUNNY_PUBLIC_CLEANUP_FAILED: {exc}", file=sys.stderr)
        sys.exit(1)
