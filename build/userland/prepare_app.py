# Copyright (C) 2020-2022 The opuntiaOS Project Authors.
#  + Contributed by Nikita Melekhin <nimelehin@gmail.com>
#
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import sys
import os
import json
import subprocess
from distutils.dir_util import copy_tree

fs_app_name = sys.argv[1]
app_name = sys.argv[2]
outpath = sys.argv[3]
src_dir = sys.argv[4]


def print_json(config_file, rdict):
    json.dump(rdict, config_file, indent=4)


def read_config(path):
    with open(path) as json_file:
        data = json.load(json_file)
        return data
    return {}


def write_config(config, outpath):
    config_file = open(outpath+"/info.json", "w")

    config['name'] = app_name
    config['exec_rel_path'] = fs_app_name
    
    # Resolve app icon
    if 'assets' in config:
        if 'AppIcon' in os.listdir(outpath + '/' + config['assets']):
            config['icon_path'] = outpath[4:] + '/' + config['assets'] + '/AppIcon'
        else:
            raise Exception("An \"AppIcon\" folder is required within the assets folder \"" + config['assets'] + "\"")
    else:
        config['icon_path'] = '/res/icons/apps/missing.icon'

    config['bundle_id'] = "com.opuntia.{0}".format(fs_app_name)

    print_json(config_file, config)
    config_file.close()


if not os.path.exists(outpath):
    os.makedirs(outpath)

config = {}
for fname in os.listdir(src_dir):
    if fname == "info.json":
        config = read_config(src_dir + "/info.json")

        if 'resources' in config:
            for resource_dir in config['resources']:
                copy_tree(src_dir + "/" + resource_dir, outpath + '/' + resource_dir)
                
        break

write_config(config, outpath)
