"""Seed a clean portable package with player settings, never game files or saves."""
import argparse
import json
from pathlib import Path


def seed(source, destination):
    ini = (source / 'bbport.ini').read_text(encoding='utf-8')
    # Absence of this key triggers desktop selection on the first launcher run.
    lines = [line for line in ini.splitlines() if line.partition('=')[0].strip() != 'output_res']
    (destination / 'bbport.ini').write_text('\n'.join(lines)+'\n', encoding='utf-8')
    settings = json.loads((source / 'user/launcher/settings.json').read_text(encoding='utf-8'))
    settings.update(game_dir='game', user_dir='', mods_dir='', patches_dir='',
                    extra_env='', frame_stats=False, gpu_profile=False, vk_validation=False,
                    check_updates=False)
    output = destination / 'user/launcher/settings.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(settings, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, required=True)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    seed(args.source, args.destination)
