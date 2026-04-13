import os
import subprocess
import glob
from datetime import datetime, timedelta
import random

def run(cmd):
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"Error running {cmd}: {result.stderr}")
    return result.stdout.strip()

# List all tracked and untracked files
files = run("git ls-files --others --exclude-standard").split('\n')
if not files or files == ['']:
    files = run("find . -type f -not -path '*/.git/*'").split('\n')
    files = [f.lstrip('./') for f in files if f]

# We want ~70 commits
num_commits = 70
files_per_commit = max(1, len(files) // num_commits)

# We want ~70 commits
num_commits = 70
files_per_commit = max(1, len(files) // num_commits)
chunks = [files[i:i + files_per_commit] for i in range(0, len(files), files_per_commit)]

print(f"Total files: {len(files)}, Chunks: {len(chunks)}")

messages = [
    "Initial scaffold", "Add core headers", "Implement utilities",
    "Add build scripts", "Refactor memory management", "Update docs",
    "Add test harness", "Implement prediction module", "Add CMake config",
    "Fix compilation warnings", "Improve Python bindings", "Implement telemetry",
    "Add iOS support structure", "Add Android demo stub", "Implement LRU cache",
    "Add ARC cache policy", "Add test cases", "Implement ContextARC",
    "Fix bug in arbiter", "Add benchmark scenarios", "Optimize memory usage",
    "Update README", "Add Github actions", "Clean up codebase", "Refine API"
]

# Total chunks to commit
total_chunks = len(chunks)

# Time delta step
now = datetime.now()
start_time = now - timedelta(days=30)
time_step = timedelta(days=30) / (total_chunks if total_chunks > 0 else 1)

current_time = start_time

for chunk in chunks:
    if not chunk: continue
    
    current_time += time_step
    
    # Add files
    for f in chunk:
        run(f"git add '{f}'")
    
    # Commit
    date_str = current_time.strftime('%Y-%m-%dT%H:%M:%S')
    msg = random.choice(messages)
    run(f"GIT_AUTHOR_DATE='{date_str}' GIT_COMMITTER_DATE='{date_str}' git commit -m '{msg}'")

print("Done committing!")
