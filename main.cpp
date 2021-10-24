#include<stdio.h>
#include<opencv2/opencv.hpp>
#include<random>
#include<iostream>


bool bPlayGame = true;
bool bFirstClick;

const int sizeMass = 30;  //マスの表示サイズ
const int sizeBlock = 35; //余白も含めた表示サイズ

cv::Mat imgMass[12];
cv::Mat gameInfoImage;
cv::Mat fieldImage;
cv::Mat baseImage;

typedef enum
{
	NotPlaying = 0,
	Playing = 1,
	Clear = 2,
	GameOver = 3,
}GameStatus;
GameStatus gameStatus;

typedef struct
{
	int x;
	int y;
}Pos;

typedef struct
{
	Pos pos;
	int event;
}Mouse;
Mouse mouse;

typedef struct
{
	std::string text;
	int secPlayTime;
	int leaveNumBomb;
	Pos pos;
}GameInfo;
GameInfo gameInfo;

typedef enum
{
	CLOSED = 0,
	OPENED = 1,
	FLAG = 2,
}MassStatus;

typedef struct
{
	bool bChengeFlag;	//マス情報変更の有無
	MassStatus massStatus;		//マス状態
	bool isBomb;			//爆弾の有無
	int numBomb;		//周囲の爆弾数
	int detectBomb;		//特定された爆弾数
	Pos pos;
	cv::Mat image;		//表示画像
}MassInfo;
MassInfo** mass;

int gameLevel;	//ゲーム難易度
int mapSizeX;	//マップの横マス数
int mapSizeY;	//マップの縦マス数
int mapNumBomb;	//マップ上の爆弾数
int mapNumLeaveMass; //開かれていないマス数


int ReadImage(std::string picturename = "MineSweeperImg.png")
{
	cv::Mat imgAllMass = cv::imread(picturename);
	if (imgAllMass.size == 0)return -1;

	cv::Rect roi;
	int width, height;
	for (int i = 0; i < 12; i++)
	{
		if (i < 6)
		{
			width = i * imgAllMass.cols / 6;
			height = 0;
		}
		else
		{
			width = (i - 6) * imgAllMass.cols / 6;
			height = imgAllMass.rows / 2;
		}

		roi = {
			cv::Point(width,height),
			cv::Size(imgAllMass.cols / 6, imgAllMass.rows / 2)
		};

		imgMass[i] = imgAllMass(roi);

		cv::Size m_size = { 30,30 };
		cv::resize(imgMass[i], imgMass[i], m_size, 0, 0);
	}

	return 0;
}

int SetGameLevel()
{
	std::cout << "GAME_LEVEL / EASY : 0 / MIDIUM : 1 / HARD : 2 / CUSTOM : 3" << std::endl;
	std::cout << "SELECT_GAME_LEVEL :";
	std::cin >> gameLevel;

	switch (gameLevel)
	{
	case 0:
		mapSizeX = 8, mapSizeY = 8, mapNumBomb = 15;
		break;
	case 1:
		mapSizeX = 16, mapSizeY = 16, mapNumBomb = 50;
		break;
	case 2:
		mapSizeX = 30, mapSizeY = 16, mapNumBomb = 99;
		break;
	case 3:
		std::cout << "INPUT FIELD_YOKO" << std::endl;
		std::cin >> mapSizeX;
		std::cout << "INPUT FIELD_TATE" << std::endl;
		std::cin >> mapSizeY;
		std::cout << "INPUT BOMB" << std::endl;
		std::cin >> mapNumBomb;
		break;
	default:
		std::cout << "Not Exists Such Game Level : " << gameLevel << std::endl;
		return -1;
	}

	//残りの爆弾数の初期設定
	mapNumLeaveMass = 0;

	return 0;
}

int Init()
{
	// ゲーム盤画像の生成
	fieldImage = cv::Mat::zeros(mapSizeY * 35, mapSizeX * 35, CV_8UC3);

	// マス情報の初期設定
	mass = new MassInfo * [mapSizeY];
	for (int y = 0; y < mapSizeY; y++)
	{
		mass[y] = new MassInfo[mapSizeX];
	}
	for (int y = 0; y < mapSizeY; y++)
	{
		for (int x = 0; x < mapSizeX; x++)
		{
			mass[y][x].bChengeFlag = false;
			mass[y][x].massStatus = CLOSED;
			mass[y][x].isBomb = false;
			mass[y][x].image = imgMass[9];
			mass[y][x].numBomb = 0;
			mass[y][x].pos = { x * 35, y * 35 };
			mass[y][x].detectBomb = 0;
			cv::Mat mat = (cv::Mat_<double>(2, 3) << 1.0, 0.0, mass[y][x].pos.x, 0.0, 1.0, mass[y][x].pos.y);
			cv::warpAffine(mass[y][x].image, fieldImage, mat, fieldImage.size(), cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
		}
	}

	gameStatus = GameStatus::Playing;
	bFirstClick = true;

	return 0;
}

int OpenMoreMass(int x, int y)
{
	if (x - 1 >= 0 && y - 1 >= 0 && mass[y - 1][x - 1].massStatus == MassStatus::CLOSED)
	{
		mass[y - 1][x - 1].bChengeFlag = true;
		mass[y - 1][x - 1].massStatus = OPENED;
		if (mass[y - 1][x - 1].numBomb == 0)OpenMoreMass(x - 1, y - 1);
	}
	if (y - 1 >= 0 && mass[y - 1][x].massStatus == MassStatus::CLOSED)
	{
		mass[y - 1][x].bChengeFlag = true;
		mass[y - 1][x].massStatus = OPENED;
		if (mass[y - 1][x].numBomb == 0)OpenMoreMass(x, y - 1);
	}
	if (x + 1 < mapSizeX && y - 1 >= 0 && mass[y - 1][x + 1].massStatus == MassStatus::CLOSED)
	{
		mass[y - 1][x + 1].bChengeFlag = true;
		mass[y - 1][x + 1].massStatus = OPENED;
		if (mass[y - 1][x + 1].numBomb == 0)OpenMoreMass(x + 1, y - 1);
	}
	if (x - 1 >= 0 && mass[y][x - 1].massStatus == MassStatus::CLOSED)
	{
		mass[y][x - 1].bChengeFlag = true;
		mass[y][x - 1].massStatus = OPENED;
		if (mass[y][x - 1].numBomb == 0)OpenMoreMass(x - 1, y);
	}
	if (x + 1 < mapSizeX && mass[y][x + 1].massStatus == MassStatus::CLOSED)
	{
		mass[y][x + 1].bChengeFlag = true;
		mass[y][x + 1].massStatus = OPENED;
		if (mass[y][x + 1].numBomb == 0)OpenMoreMass(x + 1, y);
	}
	if (x - 1 >= 0 && y + 1 < mapSizeY && mass[y + 1][x - 1].massStatus == MassStatus::CLOSED)
	{
		mass[y + 1][x - 1].bChengeFlag = true;
		mass[y + 1][x - 1].massStatus = OPENED;
		if (mass[y + 1][x - 1].numBomb == 0)OpenMoreMass(x - 1, y + 1);
	}
	if (y + 1 < mapSizeY && mass[y + 1][x].massStatus == MassStatus::CLOSED)
	{
		mass[y + 1][x].bChengeFlag = true;
		mass[y + 1][x].massStatus = OPENED;
		if (mass[y + 1][x].numBomb == 0)OpenMoreMass(x, y + 1);
	}
	if (x + 1 < mapSizeX && y + 1 < mapSizeY && mass[y + 1][x + 1].massStatus == MassStatus::CLOSED)
	{
		mass[y + 1][x + 1].bChengeFlag = true;
		mass[y + 1][x + 1].massStatus = OPENED;
		if (mass[y + 1][x + 1].numBomb == 0)OpenMoreMass(x + 1, y + 1);
	}

	return 0;
}

int SetBomb()
{
	int x = (mouse.pos.x - 100) / 35;
	int y = (mouse.pos.y - 100) / 35;

	/////////////////////////////////////////////////////
	// 周囲のマスには爆弾を配置しない
	/////////////////////////////////////////////////////
	{
		mass[y][x].isBomb = false;
		mass[y][x].bChengeFlag = true;
		mass[y][x].massStatus = OPENED;
	}
	if (x - 1 >= 0 && y - 1 >= 0) {
		mass[y - 1][x - 1].isBomb = false;
		mass[y - 1][x - 1].bChengeFlag = true;
		mass[y - 1][x - 1].massStatus = OPENED;
	}
	if (y - 1 >= 0) {
		mass[y - 1][x].isBomb = false;
		mass[y - 1][x].bChengeFlag = true;
		mass[y - 1][x].massStatus = OPENED;
	}
	if (x + 1 < mapSizeX && y - 1 >= 0) {
		mass[y - 1][x + 1].isBomb = false;
		mass[y - 1][x + 1].bChengeFlag = true;
		mass[y - 1][x + 1].massStatus = OPENED;
	}
	if (x - 1 >= 0) {
		mass[y][x - 1].isBomb = false;
		mass[y][x - 1].bChengeFlag = true;
		mass[y][x - 1].massStatus = OPENED;
	}
	if (x + 1 < mapSizeX) {
		mass[y][x + 1].isBomb = false;
		mass[y][x + 1].bChengeFlag = true;
		mass[y][x + 1].massStatus = OPENED;
	}
	if (x - 1 >= 0 && y + 1 < mapSizeY) {
		mass[y + 1][x - 1].isBomb = false;
		mass[y + 1][x - 1].bChengeFlag = true;
		mass[y + 1][x - 1].massStatus = OPENED;
	}
	if (y + 1 < mapSizeY) {
		mass[y + 1][x].isBomb = false;
		mass[y + 1][x].bChengeFlag = true;
		mass[y + 1][x].massStatus = OPENED;
	}
	if (x + 1 < mapSizeX && y + 1 < mapSizeY) {
		mass[y + 1][x + 1].isBomb = false;
		mass[y + 1][x + 1].bChengeFlag = true;
		mass[y + 1][x + 1].massStatus = OPENED;
	}

	/////////////////////////////////////////////////////
	// 爆弾の配置
	/////////////////////////////////////////////////////
	int randX, randY;
	//int leaveBomb = mapNumBomb;
	int bomb = 0;
	std::random_device rnd;

	while (bomb < mapNumBomb)
	{
		randX = rnd() % mapSizeX;
		randY = rnd() % mapSizeY;

		if (mass[randY][randX].bChengeFlag == false && mass[randY][randX].isBomb == false)
		{
			mass[randY][randX].isBomb = true;
			mass[randY][randX].image = imgMass[11];
			mass[randY][randX].numBomb = 9;
			bomb++;
		}
	}

	/*int aaa = 0;
	for (int y = 0; y < mapSizeY; y++)
	{
		for (int x = 0; x < mapSizeX; x++)
		{
			if (mass[y][x].isBomb == true)
				aaa++;
		}
	}
	std::cout << aaa << std::endl;*/

	/////////////////////////////////////////////////////
	// 周囲の爆弾数の計算
	/////////////////////////////////////////////////////
	for (int y = 0; y < mapSizeY; y++)
	{
		for (int x = 0; x < mapSizeX; x++)
		{
			if (y - 1 >= 0)
			{
				if (x - 1 >= 0)
					(mass[y - 1][x - 1].isBomb == true) ? mass[y][x].numBomb++ : 0;
				(mass[y - 1][x].isBomb == true) ? mass[y][x].numBomb++ : 0;
				if (x + 1 < mapSizeX)
					(mass[y - 1][x + 1].isBomb == true) ? mass[y][x].numBomb++ : 0;
			}
			{
				if (x - 1 >= 0)
					(mass[y][x - 1].isBomb == true) ? mass[y][x].numBomb++ : 0;
				if (x + 1 < mapSizeX)
					(mass[y][x + 1].isBomb == true) ? mass[y][x].numBomb++ : 0;
			}
			if (y + 1 < mapSizeY)
			{
				if (x - 1 >= 0)
					(mass[y + 1][x - 1].isBomb == true) ? mass[y][x].numBomb++ : 0;
				(mass[y + 1][x].isBomb == true) ? mass[y][x].numBomb++ : 0;
				if (x + 1 < mapSizeX)
					(mass[y + 1][x + 1].isBomb == true) ? mass[y][x].numBomb++ : 0;
			}
		}
	}


	if (x - 1 >= 0 && y - 1 >= 0 && mass[y - 1][x - 1].numBomb == 0)	OpenMoreMass(x - 1, y - 1);
	if (y - 1 >= 0 && mass[y - 1][x].numBomb == 0)		OpenMoreMass(x, y - 1);
	if (x + 1 < mapSizeX && y - 1 >= 0 && mass[y - 1][x + 1].numBomb == 0)	OpenMoreMass(x + 1, y - 1);
	if (x - 1 >= 0 && mass[y][x - 1].numBomb == 0)		OpenMoreMass(x - 1, y);
	if (x + 1 < mapSizeX && mass[y][x + 1].numBomb == 0)		OpenMoreMass(x + 1, y);
	if (x - 1 >= 0 && y + 1 < mapSizeY && mass[y + 1][x - 1].numBomb == 0)	OpenMoreMass(x - 1, y + 1);
	if (y + 1 < mapSizeY && mass[y + 1][x].numBomb == 0)		OpenMoreMass(x, y + 1);
	if (x + 1 < mapSizeX && y + 1 < mapSizeY && mass[y + 1][x + 1].numBomb == 0)	OpenMoreMass(x + 1, y + 1);

	return 0;
}


int OpenMass()
{
	int x = (mouse.pos.x - 100) / 35;
	int y = (mouse.pos.y - 100) / 35;

	//std::cout << "mouse x: " << x << "/ y:" << y << std::endl;

	if (mouse.event == cv::EVENT_RBUTTONDOWN)
	{
		if (mass[y][x].massStatus == FLAG)
		{
			//std::cout << "bbbbbbbbbbbbbbbbbbbbbbbbbbbb" << std::endl;

			mass[y][x].bChengeFlag = true;
			mass[y][x].massStatus = CLOSED;
			mass[y][x].detectBomb--;
			if (mapNumLeaveMass > 0)mapNumLeaveMass--;
			{
				if (x - 1 >= 0 && y - 1 >= 0) mass[y - 1][x - 1].detectBomb--;
				if (y - 1 >= 0) mass[y - 1][x].detectBomb--;
				if (x + 1 < mapSizeX && y - 1 >= 0) mass[y - 1][x + 1].detectBomb--;
				if (x - 1 >= 0) mass[y][x - 1].detectBomb--;
				if (x + 1 < mapSizeX) mass[y][x + 1].detectBomb--;
				if (x - 1 >= 0 && y + 1 < mapSizeY) mass[y + 1][x - 1].detectBomb--;
				if (y + 1 < mapSizeY) mass[y + 1][x].detectBomb--;
				if (x + 1 < mapSizeX && y + 1 < mapSizeY) mass[y + 1][x + 1].detectBomb--;
			}
		}
		else if (mass[y][x].massStatus == CLOSED)
		{

			mass[y][x].bChengeFlag = true;
			mass[y][x].massStatus = FLAG;
			mass[y][x].detectBomb++;
			mapNumLeaveMass++;
			{
				if (x - 1 >= 0 && y - 1 >= 0) mass[y - 1][x - 1].detectBomb++;
				if (y - 1 >= 0) mass[y - 1][x].detectBomb++;
				if (x + 1 < mapSizeX && y - 1 >= 0) mass[y - 1][x + 1].detectBomb++;
				if (x - 1 >= 0) mass[y][x - 1].detectBomb++;
				if (x + 1 < mapSizeX) mass[y][x + 1].detectBomb++;
				if (x - 1 >= 0 && y + 1 < mapSizeY) mass[y + 1][x - 1].detectBomb++;
				if (y + 1 < mapSizeY) mass[y + 1][x].detectBomb++;
				if (x + 1 < mapSizeX && y + 1 < mapSizeY) mass[y + 1][x + 1].detectBomb++;
			}
		}
	}
	else if (mouse.event == cv::EVENT_LBUTTONDOWN && mass[y][x].massStatus == CLOSED)
	{
		if (mass[y][x].isBomb == true)
		{
			mass[y][x].bChengeFlag = true;
			mass[y][x].massStatus = OPENED;
			gameStatus = GameOver;
		}
		else
		{
			mass[y][x].bChengeFlag = true;
			mass[y][x].massStatus = OPENED;
			//std::cout << "check x: " << x << " / y: " << y << std::endl;
			if (mass[y][x].numBomb == 0)
			{
				OpenMoreMass(x, y);
			}
		}
	}
	else if (mouse.event == cv::EVENT_LBUTTONDOWN && (mass[y][x].detectBomb == mass[y][x].numBomb))
	{
		std::cout << "detect: " << mass[y][x].detectBomb << " / bomb: " << mass[y][x].numBomb << std::endl;

		OpenMoreMass(x, y);
	}

	return 0;
}

/*
	各画像の合成・表示
*/
int Show()
{
	// ゲーム画面の生成
	baseImage = cv::Mat::zeros(mapSizeY * 35 + 200, mapSizeX * 35 + 200, CV_8UC3);

	// ゲーム情報画面の生成
	gameInfoImage = cv::Mat::zeros(100, mapSizeX * 35 + 200, CV_8UC3);



	// ゲームフィールド画面の更新
	for (int y = 0; y < mapSizeY; y++)
	{
		for (int x = 0; x < mapSizeX; x++)
		{
			if (mass[y][x].bChengeFlag == true)
			{

				if (mass[y][x].massStatus == CLOSED)mass[y][x].image = imgMass[9];
				else if (mass[y][x].massStatus == FLAG)mass[y][x].image = imgMass[10];
				else if (mass[y][x].numBomb < 9)mass[y][x].image = imgMass[mass[y][x].numBomb];
				else mass[y][x].image = imgMass[11];

				cv::Mat mat = (cv::Mat_<double>(2, 3) << 1.0, 0.0, mass[y][x].pos.x, 0.0, 1.0, mass[y][x].pos.y);
				cv::warpAffine(mass[y][x].image, fieldImage, mat, fieldImage.size(), cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);
			}
		}
	}
	cv::Mat matField = (cv::Mat_<double>(2, 3) << 1.0, 0.0, 100, 0.0, 1.0, 100);
	cv::warpAffine(fieldImage, baseImage, matField, baseImage.size(), cv::INTER_LINEAR, cv::BORDER_TRANSPARENT);


	//ゲーム画面の表示
	cv::imshow("MineSweeper", baseImage);

	return 0;
}

void mouseCallback(int event, int x, int y, int flags, void* userdata)
{
	if (event == cv::EVENT_LBUTTONDOWN || event == cv::EVENT_RBUTTONDOWN && x >= 100 && y >= 100)
	{
		mouse.pos.x = x;
		mouse.pos.y = y;
		mouse.event = event;
	}
}

int ResetStatus()
{
	// マウスイベントのリセット
	/*mouse.event = 0;*/
	// マス更新フラグのリセット
	for (int y = 0; y < mapSizeY; y++)
		for (int x = 0; x < mapSizeX; x++)
			mass[y][x].bChengeFlag = false;

	return 0;
}



int main(void)
{
	if (ReadImage())
	{
		std::cout << "READ_IMAGE_ERROR" << std::endl;
		while (1);
	}

	//全体処理
	while (bPlayGame == true)
		//while(gameStatus==GameStatus::Playing)
	{
		// 1ゲーム処理
		{
			gameStatus = GameStatus::Playing;

			// ゲームの難易度設定
			while (SetGameLevel())
			{
				std::cout << "GAMELEVEL_INPUT_ERROR: Input another number" << std::endl;
			}

			Init();

			// メイン処理
			while (1)
			{
				Show();

				if (gameStatus == GameStatus::GameOver)
				{
					std::cout << "Game Over" << std::endl;
					cv::waitKey(1500);
					break;
				}
				else if (gameStatus == GameStatus::Clear)
				{
					std::cout << "Game Clear" << std::endl;
					cv::waitKey(1500);
					break;
				}

				ResetStatus();
				cv::setMouseCallback("MineSweeper", mouseCallback);


				if (mouse.event == cv::EVENT_LBUTTONDOWN || mouse.event == cv::EVENT_RBUTTONDOWN)
				{
					if (bFirstClick == true && mouse.event == cv::EVENT_LBUTTONDOWN)
					{
						bFirstClick = false;
						SetBomb();
					}
					else
					{
						OpenMass();
					}

					mouse.event = 0;

					int cnt = 0;
					for (int y = 0; y < mapSizeY; y++)
					{
						for (int x = 0; x < mapSizeX; x++)
						{
							if (mass[y][x].isBomb == false && mass[y][x].massStatus == MassStatus::OPENED)
							{
								cnt++;
								//std::cout << " .";
							}
							else
							{
								//std::cout << " X";
							}
						}
						//std::cout<<std::endl;
					}
					std::cout << "BOMB: " << mapNumBomb - mapNumLeaveMass << std::endl;
					if (cnt == mapSizeX * mapSizeY - mapNumBomb)gameStatus = GameStatus::Clear;

					//std::cout << "Opened Mass: " << cnt << std::endl;
				}

				int key = cv::waitKey(50);
				if (key == 'q')break;
			}
		}

		cv::destroyWindow("MineSweeper");

		// 確認
		std::cout << "Start New Game ( y:1 / n:0 ): ";
		std::cin >> bPlayGame;

	}

	std::printf("\n\n Thank you for playing !!\n");

	cv::waitKey(1500);
	return 0;
}
