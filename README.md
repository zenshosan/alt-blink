<div align="center"><img src="./image/alt-blink_icon.svg" width="64"/></div>

# alt-blink
左右のaltキーを空打ちすることでIMEのon/offを切り替えるWindows用のツールです。

[alt-ime-ahk](https://github.com/karakaram/alt-ime-ahk) と同様のツールです。
個人的にWindowsのキーボードフックの使い方を知りたくなったため車輪の再発明を行いました。
ただしAutoHotkeyでなくc++でフロムスクラッチで作成しています。

## ビルド
alt-blink.vcproj がプロジェクトファイルなので適切なツールを使ってビルドしてください。
私はVS2019で開発しました。

## 使い方
alt-blink.exe を実行すると、タスクトレイにアイコンが表示される状態で起動します。
左altでIMEオフ、右altでIMEオンの動作になります。
Windowsのstartupから自動起動するようにしておくとよいと思います。

タスクトレイのアイコンを右クリックでいくつかのメニューが選択できます。
- About： バージョン等の表示
- Show LogWindow/Hide LogWindow： デバッグ用のログウィンドの表示・非表示の切り替え
- Reset： alt-blink内部のキーボードフックを再設定する (うまく動かない場合の対処療法用)
- Exit：alt-blinkを終了する

## 謝辞
前述したとおりalt-blinkは[alt-ime-ahk](https://github.com/karakaram/alt-ime-ahk) のアイデアをコピーしたツールです。
私もこれまで alt-ime-ahk にはお世話になっていました。ありがとうございます。
ただしaltキーのハンドリングのアルゴリズムは独自に書いたので振る舞いは若干異なると思います。

IME切り替えについては alt-ime-ahk も参考にしている [eamat@Cabinet - IME制御](http://www6.atwiki.jp/eamat/pages/17.html)
を参考にさせていただきました。IMEの制御についてはどうも公式のドキュメントがないようでとても助かりました。

## ライセンス
MITライセンスです。
[LICENSE](./LICENSE) を見てください。
