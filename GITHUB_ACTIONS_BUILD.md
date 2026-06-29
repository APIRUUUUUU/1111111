# GitHub Actionsでビルド済みVST3を取得する方法

このフォルダをGitHubリポジトリにアップロードすると、GitHub ActionsのWindows環境で
`Crystal Voice.vst3` を実際にビルドし、ダウンロード用ZIPとして保存します。

## 1. GitHubに新規リポジトリを作る

GitHubで `New repository` を選び、名前を `CrystalVoice-VST3` などにします。
Public / Private はどちらでも構いません。READMEの自動作成は不要です。

## 2. このフォルダの中身をアップロードする

`CrystalVoice_VST3_Prototype` フォルダの**中身**をリポジトリ直下へアップロードしてください。
以下が見える状態なら正しいです。

```text
.github/workflows/build-vst3.yml
CMakeLists.txt
Source/
```

## 3. ビルドを実行する

GitHubのリポジトリ画面で、次を開きます。

```text
Actions → Build Crystal Voice VST3 → Run workflow → Run workflow
```

pushした直後は自動実行されるため、手動実行は不要な場合があります。

## 4. ビルド済みファイルを取得する

緑のチェックで完了したワークフローを開き、画面下部の Artifacts から
`CrystalVoice-VST3-Windows` をダウンロードします。

展開すると、次のVST3バンドルが入っています。

```text
Crystal Voice.vst3
```

## 5. FL Studioへ導入する

`Crystal Voice.vst3` フォルダを、フォルダごと次にコピーします。

```text
C:\Program Files\Common Files\VST3\
```

FL Studioで以下を実行します。

```text
Options → Manage plugins → Find installed plugins
```

`Crystal Voice` が見つかれば、マイクを受けているMixer InsertのFXスロットへ挿入できます。

## 重要

GitHub Actionsでのビルドに失敗した場合は、Actionsの赤い実行結果を開き、
最初に出ているエラーのスクリーンショットを共有してください。
