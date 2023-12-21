#!/lusr/bin/python3

# Created by Ojas Phirke

import os
import sys
import subprocess
    

def main():
    global ENGINE_LOCATION
    args = sys.argv[1:]
    args = [ENGINE_LOCATION] + args
    subprocess.run(args, text=True)
    
ENGINE_LOCATION = os.path.join("/u/ojasp/Public/os", "engine_backend.py")

if __name__ == "__main__":
    main()
    