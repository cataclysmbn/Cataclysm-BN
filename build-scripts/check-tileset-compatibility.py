#!/usr/bin/env python3
"""Reject unchecked UltiCa-BN archives before bundling them with the game."""
import argparse
import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat
import zipfile

PACKS = {'UltimateCataclysm': 'UltimateCataclysm', 'ChibiUltica': 'Chibi_Ultica', 'Altica': 'Altica'}
NAMES = {'UltimateCataclysm': 'UltimateCataclysm', 'ChibiUltica': 'Chibi_Ultica',
         'Altica': 'Alternative Ultica'}


def verify_archives(archives):
    checksums = {}
    for line in (archives / 'SHA256SUMS').read_text().splitlines():
        digest, name = line.split()
        name = name.removeprefix('*')
        if not re.fullmatch(r'[0-9a-f]{64}', digest) or name in checksums:
            raise ValueError('Invalid or duplicate archive checksum')
        checksums[name] = digest
    for folder in PACKS:
        archive = folder + '.zip'
        if hashlib.sha256((archives / archive).read_bytes()).hexdigest() != checksums.get(archive):
            raise ValueError(f'Archive checksum mismatch: {archive}')
        with zipfile.ZipFile(archives / archive) as z:
            for entry in z.infolist():
                path = PurePosixPath(entry.filename)
                if (path.is_absolute() or '..' in path.parts or not path.parts
                        or path.parts[0] != folder or '\\' in entry.filename
                        or stat.S_ISLNK(entry.external_attr >> 16)):
                    raise ValueError(f'Unsafe archive entry: {archive}/{entry.filename}')


def check(root, archives, release, contract):
    if not re.fullmatch(r'ultica-bn-[0-9a-f]{12}', release):
        raise ValueError('A pinned UltiCa-BN release is required')
    verify_archives(archives)
    commits = set()
    for folder, pack in PACKS.items():
        path = root / folder
        receipt = json.loads((path / 'BN_COMPATIBILITY.json').read_text())
        if (receipt.get('schema') != 2 or receipt.get('pack') != pack
                or receipt.get('contract_sha256') != contract
                or receipt.get('stock_reference') != '2026-03-29'
                or receipt.get('renderer_features') != ['rotates_to']
                or receipt.get('bn_pr') != 'https://github.com/cataclysmbn/Cataclysm-BN/pull/10977'):
            raise ValueError(f'Unreviewed BN compatibility receipt: {folder}')
        files = receipt.get('files', {})
        if not {'tile_config.json', 'tileset.txt', 'layering.json', 'BN_ORIENTATION.json'} <= files.keys():
            raise ValueError(f'Incomplete compatibility receipt: {folder}')
        for name, digest in files.items():
            file = Path(name)
            if file.is_absolute() or '..' in file.parts:
                raise ValueError('Invalid receipt path')
            if hashlib.sha256((path / file).read_bytes()).hexdigest() != digest:
                raise ValueError(f'Changed pack file: {folder}/{name}')
        descriptor = {}
        for line in (path / 'tileset.txt').read_text().splitlines():
            key, separator, value = line.partition(':')
            if separator and key.strip() in {'NAME', 'VIEW', 'JSON'}:
                descriptor[key.strip()] = value.strip()
        if descriptor.get('NAME') != NAMES[folder] or descriptor.get('JSON') != 'tile_config.json':
            raise ValueError(f'Unexpected pack descriptor: {folder}')
        config = json.loads((path / 'tile_config.json').read_text())
        if not {s['file'] for s in config['tiles-new']} <= files.keys():
            raise ValueError(f'Unverified tilesheet: {folder}')
        match = re.search(r'^Commit: ([0-9a-f]{40})$', (path / 'SOURCE.txt').read_text(), re.MULTILINE)
        if not match or release != 'ultica-bn-' + match[1][:12]:
            raise ValueError(f'Release/source mismatch: {folder}')
        commits.add(match[1])
    if len(commits) != 1:
        raise ValueError('Mixed source revisions')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gfx', type=Path)
    parser.add_argument('--archives-only', action='store_true')
    parser.add_argument('--archives', type=Path, required=True)
    parser.add_argument('--release')
    parser.add_argument('--contract')
    args = parser.parse_args()
    if args.archives_only:
        verify_archives(args.archives)
        print('All UltiCa-BN archive checksums and paths verified')
    else:
        if not (args.gfx and args.release and args.contract):
            parser.error('--gfx, --release and --contract are required for pack verification')
        check(args.gfx, args.archives, args.release, args.contract)
        print('All UltiCa-BN archives and compatibility receipts verified')


if __name__ == '__main__':
    main()
