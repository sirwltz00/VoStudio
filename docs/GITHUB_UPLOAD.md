# Upload this engine update to `sirwltz00/VoStudio`

The repository connected to the ChatGPT GitHub integration is readable, but that integration currently rejects content and Git-object writes with HTTP 403 (`Resource not accessible by integration`). This package is therefore prepared for a normal Git push or upload through GitHub's own interface.

## With Git installed

1. Download and extract `VoStudio-DSP-Engine-Alpha.zip`.
2. Clone the repository:

   ```sh
   git clone https://github.com/sirwltz00/VoStudio.git
   cd VoStudio
   ```

3. Copy the extracted package contents into this repository directory, including hidden items such as `.github/`.
4. Commit and push:

   ```sh
   git add -A
   git commit -m "Implement VoStudio DSP engine alpha"
   git branch -M main
   git push -u origin main
   ```

If your repository already has commits, the push will use its current `main` branch. Do not overwrite unrelated work; inspect `git status` and `git log` first.

## With GitHub Codespaces

An empty GitHub repository may need its first commit before GitHub offers a Codespace. Create a small `README.md` commit from the GitHub website, create a Codespace on `main`, upload the ZIP into the Codespaces file explorer, then run:

```sh
unzip -o VoStudio-DSP-Engine-Alpha.zip -d /tmp/vostudio-engine-update
cp -a /tmp/vostudio-engine-update/. .
git add -A
git commit -m "Implement VoStudio DSP engine alpha"
git push origin main
```

Review the staged files before committing. The archive excludes local build folders and generated render files.
