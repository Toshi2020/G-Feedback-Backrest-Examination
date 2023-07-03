# G-Feedback-Backrest-Examination
# Ｇフィードバック・バックレスト(背もたれ)の検討

●ここには詳細を置きます。概要はこちらのリンク先↓に記述しました。  
https://minkara.carview.co.jp/userid/3336538/blog/47065790/

動作の状況の動画はこちら。  
https://youtu.be/VhTnSI_Qglo

●ハードウェア  
![回路図](https://github.com/Toshi2020/G-Feedback-Backrest-Examination/assets/81674805/2cc291c0-6cd0-4264-a617-a5b8daed87d7)

・サーボモーターへの指示ポートにはプルアップ抵抗を入れています。手持ちが8個入りのモジュール抵抗しかなかったので、中央でポキッと折って基板の裏に付けています(褒められるやり方ではないです)。プルアップしないとフラッシュ書き込みやリセット時にアームが一瞬動いてしまいます。逆側に動くとアームが取り付けボードに当たって拘束されストールする恐れがあります。  
・サーボモーター接続用のピンソケットはArduino NANOに直付けしています。  
・タクトSWも基板に直付けしていますが、コールド側もポートに接続してL出力とすることでGNDレベルとしています。  
![NANO写真](https://github.com/Toshi2020/G-Feedback-Backrest-Examination/assets/81674805/4b2d232c-a1b2-45a1-948e-c94bd22a8105)


・サーボモーター保持ブラケットとアームは3Dプリンタで印刷しています。両面テープと木ネジでMDFボードに取り付けました。アームは25Tのアルミ製円形サーボホーンとM3x10mmネジ4本で固定しています。  
・ArduinoやDCジャックはホットボンドで取り付けています。  
・100均の玄関マットをカットしてマジックテープで貼り付け、やはり100均のクッションから中綿を抜いたカバーを適当に縫ってサイズを縮め、ファスナーも縫い付けてダボッとしたカバーを作成しました。  
![全体(補正)](https://github.com/Toshi2020/G-Feedback-Backrest-Examination/assets/81674805/5155bfd0-be7d-4842-94f2-773cf2b9790d)
![カーペット](https://github.com/Toshi2020/G-Feedback-Backrest-Examination/assets/81674805/b7988287-038a-4557-9625-c4558e843ebf)
![カバー](https://github.com/Toshi2020/G-Feedback-Backrest-Examination/assets/81674805/11e36c61-fbcf-4ddd-8f94-894b2d1f6a31)
![セット後全体(補正)](https://github.com/Toshi2020/G-Feedback-Backrest-Examination/assets/81674805/d11323cb-ee87-4315-a897-93a3e0c7f4a9)

●Arduino側ソフト  
・ソースフォルダ：Arduino\G-FeedBackRest  
・サーボモーターにばらつきがあるので組み付ける前に中点を調整します。#define TESTを1にしてビルドし、シリアルモニタで1500前後の値を入れて、各サーボモーターが90°になる値をTrim[]に設定します。TEST=2と3は確認用です。終わったら0に戻してビルドしておきます。  
・受信データはカンマ区切りでインターバル[s]、前後加速度[G]、左右加速度[G]、上下加速度[G]CRLFです。上下加速度には重力成分は含めません。  
・前後加速度は前が正、左右加速度は右が正、上下加速度は上が正としています。  
・ここでいう加速度は車体が発生している加速度なので、遠心力や重力に抗う向きに出ると考えます。加速時や上向きのピッチでで前方向に、右旋回や左バンクで右方向に、ループ飛行では上方向に出ることになります。それぞれの軸のプラス側に出ていれば正の値となります。  
・インターバルは受信していますが未使用です。  
・1行に含まれるカンマ','の数が3つであることを確定条件にしています。  
・SimToolsを使う時はインターバルの代わりにロールとピッチを受けることとし、カンマが4つであることで切り分けています。結局SimToolsは使わなかったので受信後の処理は中途半端なままです。  
・基本的にＧと逆側のサーボモーターを前に動かします。例えば加速時は後ろ側、すなわちすべてのアームが前に動きます。  
・ただし上下Ｇに関しては、マイナスＧで上側を引っ込める動きとしています。その方が抜けるフィールと合っている気がしたので。  
・人体がＧによりシートから受ける力は、体とシートが弾性体とすればバネモデルと考えられるので、PCから受け取ったＧに1次遅れのフィルタを入れてサーボモーターに指示しています。物理的に求めた値ではないのであくまで雰囲気ですが。  
・サーボモーターへの出力はPCからのデーター周期とは非同期の10msループで行っています。100msインターバルにすると動作時に「ジジジジ、、、」と音が出るので。  
・起動時と通信タイムアウトでゼロＧのアーム位置まで徐々に動かすようにしています。  
・現状アーム角度が0～90［°]で-2.0～+4.0[G]に対応するようにしています。  
・タクトSWを押すとアームを最後端まで戻して格納するようにしています。電源を切る前に操作するかと思って入れたのですが、電源オフだとアームを押せば動くことが分かったのであまり意味がなかったかもしれません。戻り切った状態でもう一度押すと通常動作に戻ります。  
・サーボモーター側の不感帯が大きいようで、指示をジワーッと変化させても、カクン、カクンという動きにしかなりません。サーボモーターってそういうものなのか、あるいはコンパチ品だからなのかもしれません。  

●Windows側ソフト  
・シミュレーターアプリからデーターを取得する方法は、これまで調べた限り大きく分けて3つありました。  
①コールバックルーチンを登録するタイプ：Microsoft Flight Simulator X  
②UDPソケットでデータフレームを送ってもらうタイプ：FlightGear、Live for Speed  
③Shared Memoryを読みに行くタイプ：RaceRoom Racing Experience、Assetto Corsa  
・開発は無料のVisual Studio 2022を使ってコンソールアプリで組んでます。  
・シリアルポートを指定するために、COMx.txtというファイルにポート(COM1～COM9)を記述してexeファイルと同じ場所に置く仕様としました。Visual Studioから実行するときはソースと同じ場所に必要です。  
・シリアルのビットレートは115200bpsに固定です。  
・送信データはカンマ区切りでインターバル[s]、前後加速度[G]、左右加速度[G]、上下加速度[G]CRLFです。上下加速度には重力成分は含めません。  
・データの送信は約100msごととしています。オーバーヘッドがあるのできっちり100msにはなっていません。Android側はこれとは非同期のインターバルで回すので、送信側の周期はあまり遅くなければ適当でもいいはずです。  
・3秒以上同じ値が連続したら送信を停止するようにしています。シミュレータを中断した後で最後の値が延々と続く場合に、Arduino側でタイムアウトを判定してサーボモーターをゼロＧ位置まで徐々に戻すためです。  
・コンソールにもデータを出力しているので、コピペしてエディタに張り付けcsvとして保存してEXCELで読み込んで値の内容をチェックしました。  

★Microsoft Flight Simulator X  
・SimConnectと呼ばれているライブラリによるAPIでコールバックにより値を受け取ります。  
・ソースフォルダ：Windows\SimConnect\FsimGetG  
・ビルドするためにはSDKフォルダよりSimConnect.hとSimConnect.libをソースと同じ場所にコピーしてください。  
・SimConnectのSDKはDVD版の場合1枚目のSDKフォルダの中にインストーラーがあります。C:\Program Files (x86)\Microsoft Games\Microsoft Flight Simulator X SDKフォルダにインストールされます。  
・その下の\Core Utilities Kit\SimConnect SDK\lib\SimConnect.lib"と\Core Utilities Kit\SimConnect SDK\inc\SimConnect.h"が必要となるファイルです。  
・Steam版の場合はSDKは自動的にインストールされ、場所はSteamLibrary\steamapps\common\FSX\SDKになります。  
・DVD版とSteam版ではライブラリが異なるようで、互換性がありませんでした。それぞれの.hと.libを使って専用にビルドする必要がありました。  
・32ビットアプリのようで、Visual Studioでのソリューションプラットフォームとしてはx86を選択する必要があります。FlightSimulator2020のSDKのライブラリを使う場合はx64を選択する必要があるようです。  
・サンプルコードとしてはSDKフォルダの下にRequestDataがあります。  
・読み出したい変数を最初にSimConnect_AddToDataDefinition()で順次設定します。  
・変数はSDKフォルダ下のドキュメントよりこちらの方が新しいようです。  
http://www.prepar3d.com/SDK/Core%20Utilities%20Kit/Variables/Simulation%20Variables.html#Aircraft%20Position%20And%20Speed%20Data  
これによれば  
"PLANE PITCH DEGREES"   ピッチ角[rad]  
"PLANE BANK DEGREES"    バンク角[rad]  
"ACCELERATION BODY X"   左右加速度[feet/s^2]  
"ACCELERATION BODY Y"   上下加速度[feet/s^2]  
"ACCELERATION BODY Z"   前後加速度[feet/s^2]  
です。が、  
・左右加速度は説明文ではin east/west direction、前後加速度は in north/south directionとありますが、方位には無関係のローカルな自分の座標に対しての値のようで、これは説明文の方が間違っていると思われます。  
・また上下加速度は値自体が変で、加速度の微分値(ジャーク)が出ているようです。実際VELOCITY BODY Yが重力込みの加速度のようで、これを微分するとACCELERATION BODY Yに一致しました。  
・仕様と異なる値を使いたくはないので、上下加速度に関してはGメーターの値である
"G FORCE"を使うことにしました。これには重力加速度が含まれるので、得られた値からcos(Pitch) * cos(Bank)を差し引いて運動により加速度としています。これで正しいのかはわかりませんがそれっぽい値にはなっているようです。  
・SimConnect_CallDispatch()をループ内で繰り返し呼び出してコールバックルーチンがデーターをアップデートしてくれるのを待つ必要があるとのことです。  
https://docs.flightsimulator.com/html/Programming_Tools/SimConnect/API_Reference/General/SimConnect_CallDispatch.htm  

★FlightGear 2020.3  
・シリアル出力もできるようですが、今回はUDPでデータを送ってもらいます。  
・ソースフォルダ：Windows\UDP\FgGetG  
・参考にしたのは以下のリンク  
Generic protocol  
https://wiki.flightgear.org/Generic_protocol  
Property tree  
https://wiki.flightgear.org/Property_tree  
Property browser  
https://wiki.flightgear.org/Property_browser  
Command line options  
https://wiki.flightgear.org/Command_line_options  
・変数に関してのドキュメントは存在しないようです。機体モデルによって変数の内容が異なるようなので。  
・シミュレーション画面で'/'キーを押すと変数ブラウザの画面がポップアップし、リアルタイムの変数の内容を見ることができます。ここからそれっぽい変数名と型をメモっておきます。この変数が正解かどうかはわかりませんが。  
・セスナの場合  
"/orientation/heading-deg"                  方位角[deg]...未使用  
"/orientation/pitch-deg"                    ピッチ角[deg]  
"/orientation/roll-deg"                     ロール角[deg]  
"/fdm/jsbsim/accelerations/n-pilot-x-norm"  前後加速度[G]  
"/fdm/jsbsim/accelerations/n-pilot-y-norm"  左右加速度[G]  
"/fdm/jsbsim/accelerations/n-pilot-z-norm"  上下加速度[G]  
これをxmlファイルに記述します。今回はFgGetG.xmlとして用意しました。  
・このXMLをFlightGearをインストールしたフォルダの下の\data\Protocolフォルダにコピーします。  
・FlightGearのショートカットを作成しコマンドラインオプションに以下を追加して起動します。  
--generic=socket,out,100,127.0.0.1,16661,udp,FgGetG  
・この変数はセスナの場合なので、他の機体モデルだと変数自体が存在しないか値が入らない可能性はあります。例えばUFOでは加速度は0です(実際発生したら中の宇宙人もつぶれちゃうでしょうが)  

★Live for Speed(LFS)  
・InSim/OutSimと呼ばれるAPIを使いますが、中身はUDPでの通信です。  
・ソースフォルダ：Windows\InSim\LfsGetG  
・参考にしたのは以下のリンク  
https://en.lfsmanual.net/wiki/OutSim_/_OutGauge  
https://en.lfsmanual.net/wiki/InSim  
https://www.lfs.net/forum/9-LFS-Programmer-Forum  
・LFS/docフォルダのInSim.txtがドキュメントを兼ねたヘッダーファイルになってます。今回はそこから必要な部分をコピーしました。  
・読み取るのは以下の変数となります。  
pack.Heading        方位角[rad]  
pack.Pitch          ピッチ[rad]  
pack.Roll           ロール[rad]  
pack.Accel.x        左右加速度[m/s^2]  
pack.Accel.y        前後加速度[m/s^2]  
pack.Accel.z        上下加速度[m/s^2]  
・前後と左右の加速度はワールド座標なので、Headingに基づいて回転させてます。  
・LFSインストールフォルダのcfg.txtの内容を以下のように変更します。これでUDP送信を行うようになります。ポート番号は空いているならこの番号でなくてもいいですが、ソフト側も変更してリビルドする必要があります。  
    OutSim Mode 1  
    OutSim Delay 1  
    OutSim IP 127.0.0.1  
    OutSim Port 4123  
    OutSim ID 1  
    OutSim Opts 0  

★RaceRoom Racing Experience(R3E)  
・Shared Memoryによりデータを取得します。  
・ソースフォルダ：Windows\SharedMemory\R3eGetG  
・参考にしたのは以下のリンク  
Shared Memory API  
https://forum.kw-studios.com/index.php?threads/shared-memory-api.1525/  
・ビルドするためにはこちら↓のCサンプルソースからr3e.hをローカルにコピーします。  
https://github.com/sector3studios/r3e-api  
・読み取るのは以下の変数となります。  
angle = Map_buffer->car_orientation;  
accel = Map_buffer->local_acceleration;  
angle.pitch     ピッチ[rad]  
angle.roll      ロール[rad]  
accel.x         左右加速度[m/s^2]  
accel.y         上下加速度[m/s^2]  
accel.z         前後加速度[m/s^2]  

★Assetto Corsa(AC)  
・UDPとShared Memoryの両方のAPIが用意されていますが、今回はShared Memoryによりデータを取得します。  
・ソースフォルダ：Windows\SharedMemory\AcGetG  
・参考にしたのは以下のリンク  
Shared Memory Reference ACC  
https://assettocorsamods.net/threads/doc-shared-memory-reference-acc.3061/  
・読み取るのは以下の変数となります。  
pf->heading     方位角[rad]...未使用  
pf->pitch       ピッチ[rad]  
pf->roll        ロール[rad]  
pf->accG[0]     左右加速度[G]  
pf->accG[1]     上下加速度[G]  
pf->accG[2]     前後加速度[G]   

★★★ SimGetG ★★★  
・シミュレーションごとに別々のWindows側のソフトを選んで起動するのは面倒なので、ここまでのソフトを束ねました。  
・ソースフォルダ：Windows\SimGetG  
・プロセスを監視して起動したシミュレータとの通信を行います。  
・ビルドするためにはローカルにSimConnect.hとSimConnect.libとr3e.hをコピーする必要があります。  
・SimConnectが32ビットなのでソリューションプラットフォームとしてx86を選択する必要があります。  
・COMポートの指定は今まで同様COMx.txtで行います。  

●3Ddata  
・一応3Dプリンタ用のデータを置きました。  

