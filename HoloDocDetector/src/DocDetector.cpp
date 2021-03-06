#ifdef _DLL_BUILD
#include "stdafx.h"
#endif
#ifdef _DLL_UWP_BUILD
#include "pch.h"
#endif

#include "DocDetector.hpp"
#include <set>
#include <vector>

#include <opencv2/imgproc.hpp>
// Meant to disapear (used for simple document detection just to be able to print)
#include <opencv2/highgui.hpp>

using namespace std;
using namespace cv;

//**********************************
//********** DECLARATIONS **********
//**********************************
//*****************
//***** CONST *****
//*****************
const vector<Scalar> COLORS = {
	Scalar(0, 0, 0), Scalar(125, 125, 125), Scalar(255, 255, 255),	// Noir		Gris		Blanc
	Scalar(255, 0, 0), Scalar(0, 255, 0), Scalar(0, 0, 255),	// Rouge	Vert		Bleu
	Scalar(0, 255, 255), Scalar(255, 0, 255), Scalar(255, 255, 0),	// Cyan		Magenta		Jaune
	Scalar(255, 125, 0), Scalar(0, 255, 125), Scalar(125, 0, 255),	// Orange	Turquoise	Indigo
	Scalar(255, 0, 125), Scalar(125, 255, 0), Scalar(0, 125, 255),	// Fushia	Lime		Azur
	Scalar(125, 0, 0), Scalar(0, 125, 0), Scalar(0, 0, 125)		// Blood	Grass		Deep
};

const int COLOR_RANGE = 25;

const double RATIO_LENGTH_MIN = 0.1,	//10% of perimeter
			 RATIO_LENGTH_MAX = 0.7,	//70% of perimeter
			 RATIO_SIDE = 0.5;			//50% between two sides (it's huge now we used Squared Distance)
//****************
//***** Misc *****
/// <summary>Sorts the area.</summary>
/// <param name="a">Point a.</param>
/// <param name="b">Point b.</param>
/// <returns><c>True</c> if area(a) bigger than area(b), <c>False</c> if not</returns>
static bool sortArea(const vector<Point> &a, const vector<Point> &b);

/// <summary>Sorts the points by x.</summary>
/// <param name="a">Point a.</param>
/// <param name="b">Point b.</param>
/// <returns><c>True</c> if a.x lower than b.x, <c>False</c> if not</returns>
static bool sortPointsX(const Point &a, const Point &b);

/// <summary>Sorts the points by y.</summary>
/// <param name="a">Point a.</param>
/// <param name="b">Point b.</param>
/// <returns><c>True</c> if a.y bigger than b.y, <c>False</c> if not</returns>
static bool sortPointsY(const Point &a, const Point &b);

static void sortContourPoints(const vector<Point> &in, vector<Point> &out);

/// <summary>Verify if x is in the range.</summary>
/// <param name="x">The x.</param>
/// <param name="min">The minimum.</param>
/// <param name="max">The maximum.</param>
/// <returns><c>True</c> if x between min and max, <c>False</c> if not</returns>
static bool inRange(double x, double min, double max);

/// <summary>Gets the color of the range.</summary>
/// <param name="background">The background.</param>
/// <param name="lower">The lower.</param>
/// <param name="higher">The higher.</param>
/// <param name="range">The range.</param>
static void GetRangeColor(const Scalar &background, Scalar &lower, Scalar &higher, int range = COLOR_RANGE);

/// <summary>Gets the center.</summary>
/// <param name="v">Vector of point (contours for example).</param>
/// <returns>Center of vector of point.</returns>
static Point GetCenter(const vector<Point> &v);

/// <summary>Squared distance between two points (or points to the origin).</summary>
/// <param name="a">Point a.</param>
/// <param name="b">Point b.</param>
/// <returns>Squared distance between the two points.</returns>
static int SquaredDist(const Point &a, const Point &b = Point(0, 0));

/// <summary>Distance between two points (or points to the origin).</summary>
/// <param name="a">Point a.</param>
/// <param name="b">Point b.</param>
/// <returns>Distance between the two points.</returns>
static double Dist(const Point &a, const Point &b = Point(0, 0));

/// <summary>Cross product of two vectors.</summary>
/// <param name="a">Point a.</param>
/// <param name="b">Point b.</param>
/// <returns>int for the Cross product of two vectors (type point to avoid conversion).</returns>
static int CrossProduct(const Point &a, const Point &b);

static vector<Point> GetCenterestContour(const vector<vector<Point>> &contours, const Point &center);

//****************

//**********************
//***** Conversion *****
/// <summary>
/// Unity image to OpenCV Mat.
/// </summary>
/// <param name="image">Unity image.</param>
/// <param name="height">Image height.</param>
/// <param name="width">Image width.</param>
/// <param name="dst">tri-channel 8-bit image.</param>
/// <return>Error Code (<see cref = "ERROR_CODE"/>).</return>
static int UnityToOpenCVMat(Color32 *image, uint height, uint width, Mat &dst);

/// <summary>
/// Unity image to OpenCV Mat, but here it is the inverse.
/// </summary>
/// <param name="input">tri-channel 8-bit image.</param>
/// <param name="output">Unity image.</param>
/// <return>Error Code (<see cref = "ERROR_CODE"/>).</return>
static int OpenCVMatToUnity(const Mat &input, byte *output);

/// <summary>
/// Docses to unity.
/// </summary>
/// <param name="docs">Vector of Documents. Each doc is represented by a 8-elements vector \f$(x_1, y_1, x_2, y_2, x_3, y_3, x_4, y_4)\f$,
/// where \f$(x_1,y_1)\f$, \f$(x_2, y_2)\f$, \f$(x_3, y_3)\f$ and \f$(x_4, y_4)\f$ are the corner of each detected Document.</param>
/// <param name="dst">The DST.</param>
/// <param name="nbDocs">Number of documents.</param>
/// <return>Error Code (<see cref = "ERROR_CODE"/>).</return>
static int DocsToUnity(vector<Vec8i> &docs, int *dst, uint &nbDocs);

/// <summary>Unity Color to OpenCV Color.</summary>
/// <param name="in">Unity Color</param>
/// <param name="out">OpenCV Color.</param>
static void Color32ToScalar(const Color32 &in, Scalar &out);

/// <summary>Format Contours to vector of coordonnate for Unity.</summary>
/// <param name="contours">The contours.</param>
/// <param name="docs">The docs.</param>
/// TODO:Combine with the DocsToUnity Function
static void ContoursToDocs(const vector<vector<Point>> &contours, vector<Vec8i> &docs);
//**********************

//****************************
//***** Image Processing *****

/// <summary>Binarisations the specified source.</summary>
/// <param name="src">tri-channel 8-bit input image.</param>
/// <param name="dst">single-channel 8-bit binary image.</param>
/// <param name="background">The background color.</param>
/// <param name="range">The color range.</param>
/// <return>Error Code (<see cref = "ERROR_CODE"/>).</return>
static int Binarisation(const Mat &src, Mat &dst, const Scalar &background = COLORS[0], int range = COLOR_RANGE);

/// <summary>Find Contours.</summary>
/// <param name="src">single-channel 8-bit binary image.</param>
/// <param name="contours">The contours.</param>
/// <param name="length_min">The length minimum of contours.</param>
/// <param name="length_max">The length maximum of contours.</param>
/// <return>Error Code (<see cref = "ERROR_CODE"/>).</return>
static int DocsContours(const Mat &src, vector<vector<Point>> &contours,
						double length_min, double length_max);

/// <summary>Extract 4 corners of a contour.</summary>
/// <param name="contour">The contour.</param>
/// <param name="length_min">The length minimum.</param>
/// <param name="length_max">The length maximum.</param>
/// <returns><c>True</c> if good length quad is found, <c>False</c> if not</returns>
static bool Extract4Corners(vector<Point> &contour, double length_min, double length_max);

/// <summary>Verify if the point is on the Quad.</summary>
/// <param name="quad">The quad.</param>
/// <param name="point">The point.</param>
/// <returns><c>True</c> if the poitn is in the quad, <c>False</c> if not</returns>
static bool inQuad(const vector<Point> &quad, const Point &point);

static int UnDistord(const Mat &src, vector<Point> &contour, Mat &dst);
//****************************

//*********************************
//********** DEFINITIONS **********
//*********************************
//****************
//***** Misc *****
bool sortArea(const vector<Point> &a, const vector<Point> &b) { return contourArea(a) > contourArea(b); }
bool sortPointsX(const Point &a, const Point &b) { return a.x < b.x; }
bool sortPointsY(const Point &a, const Point &b) { return a.y > b.y; }

void sortContourPoints(const vector<Point> &in, vector<Point> &out)
{
	// Contours is in counter clockwise order we check top left point and 
	// reorganise after that to have Top-left top right bottom right bottom left
	int Min = in[0].x + in[0].y;
	int Id = 0;
	for (int i = 1; i < 4; ++i) {
		const int Sum = in[i].x + in[i].y;
		if (Sum < Min) {
			Min = Sum;
			Id = i;
		}
	}
	out.clear();
	out.reserve(4);
	out.push_back(in[Id]);
	for (int i = 1; i < 4; ++i) {
		int j = Id - i;
		if (j < 0) j += 4;
		out.push_back(in[j]);
	}
}

bool inRange(const double x, const double min, const double max)
{
	return min <= x && x <= max;
}

void GetRangeColor(const Scalar &background, Scalar &lower, Scalar &higher, int range)
{
	if (range > 127) {
		range = 127;
	}

	for (int i = 0; i < 3; ++i) {
		lower[i] = background[i] - range;
		higher[i] = background[i] + range;
		if (lower[i] < 0) {
			higher[i] -= lower[i];
			lower[i] = 0;
		}
		if (higher[i] > 255) {
			lower[i] -= higher[i] - 255;
			higher[i] = 255;
		}
	}
}

Point GetCenter(const vector<Point> &v)
{
	const int Nb_points = int(v.size());
	Point Center(0, 0);
	for (const Point &p : v) {
		Center += p;
	}
	Center.x /= Nb_points;
	Center.y /= Nb_points;
	return Center;
}

int SquaredDist(const Point &a, const Point &b)
{
	const Point V = a - b;
	return V.x * V.x + V.y * V.y;
}

double Dist(const Point &a, const Point &b)
{
	return sqrt(SquaredDist(a, b));
}

int CrossProduct(const Point &a, const Point &b)
{
	return a.x * b.y - a.y * b.x;
}

vector<Point> GetCenterestContour(const vector<vector<Point>> &contours, const Point &center)
{
	if (contours.empty()) return vector<Point>();
	int Id = 0;
	if (contours.size() > 1) {
		int Dist = SquaredDist(GetCenter(contours[0]) - center);
		for (uint i = 1; i < contours.size(); ++i) {
			const int D = SquaredDist(GetCenter(contours[i]) - center);
			if (D < Dist) {
				Dist = D;
				Id = i;
			}
		}
	}
	return contours[Id];
}
//****************

//**********************
//***** Conversion *****

int UnityToOpenCVMat(Color32 *image, uint height, uint width, Mat &dst)
{
	dst = Mat(height, width, CV_8UC4, image);
	if (dst.empty()) return EMPTY_MAT;
	cvtColor(dst, dst, CV_RGBA2BGR);
	return NO_ERRORS;
}

int OpenCVMatToUnity(const Mat &input, byte *output)
{
	if (input.empty()) return EMPTY_MAT;
	Mat tmp;
	cvtColor(input, tmp, CV_BGR2RGB);
	memcpy(output, tmp.data, tmp.rows * tmp.cols * 3);
	return NO_ERRORS;
}

int DocsToUnity(vector<Vec8i> &docs, int *dst, uint &nbDocs)
{
	if (docs.empty()) return NO_DOCS;

	uint index = 0;
	for (uint i = 0; i < nbDocs; i++) {
		Vec8i doc = docs[i];
		for (uint j = 0; j < 8; j++) {
			dst[index++] = doc[j];
		}
	}

	return NO_ERRORS;
}

void Color32ToScalar(const Color32 &in, Scalar &out) { out = Scalar(in.b, in.g, in.r); }

void ContoursToDocs(const vector<vector<Point>> &contours, vector<Vec8i> &docs)
{
	docs.resize(contours.size());
	for (int i = 0; i < int(contours.size()); ++i) {
		for (int j = 0; j < 4; ++j) {
			docs[i][2 * j] = contours[i][j].x;
			docs[i][2 * j + 1] = contours[i][j].y;
		}
	}
}
//**********************

//****************************
//***** Image Processing *****
int Binarisation(const Mat &src, Mat &dst, const Scalar &background, int range)
{
	if (src.empty()) return EMPTY_MAT;
	if (src.type() != CV_8UC3) return TYPE_MAT;
	Scalar Lower, Higher;
	GetRangeColor(background, Lower, Higher, range);
	inRange(src, Lower, Higher, dst);
	return NO_ERRORS;
}

int DocsContours(const Mat &src, vector<vector<Point>> &contours,
				 const double length_min, const double length_max)
{
	//Find Contours
	findContours(src, contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);
	if (contours.empty()) return NO_DOCS;

	//First Vérification
	auto Begin = contours.begin();
	for (int i = 0; i < int(contours.size()); ++i) {
		const double peri = arcLength(contours[i], true);
		if (contours[i].size() < 4 || !inRange(peri, length_min, length_max)) {
			contours.erase(Begin + i);
			i--;
		}
	}
	if (contours.empty()) return NO_DOCS;

	//Extract Corners
	for (int i = 0; i < int(contours.size()); ++i) {
		if (!Extract4Corners(contours[i], length_min, length_max)) {
			contours.erase(Begin + i);
			i--;
		}
	}
	if (contours.empty()) return NO_DOCS;

	//Shape Verification
	//the opposite side must have same distance (with a huge threshold)
	//(we use the square of the distance and a deformation due to the detection and the camera is present)
	const double Ratio_min = 1 - RATIO_SIDE, Ratio_max = 1 + RATIO_SIDE;
	Begin = contours.begin();
	for (int i = 0; i < int(contours.size()); ++i) {
		int Sides[4];
		Sides[0] = SquaredDist(contours[i][0], contours[i][1]);
		Sides[1] = SquaredDist(contours[i][2], contours[i][3]);
		Sides[2] = SquaredDist(contours[i][1], contours[i][2]);
		Sides[3] = SquaredDist(contours[i][3], contours[i][0]);
		const double Ratio_1 = 1.0 * Sides[0] / Sides[1],
					 Ratio_2 = 1.0 * Sides[2] / Sides[3];
		if (!inRange(Ratio_1, Ratio_min, Ratio_max) || !inRange(Ratio_2, Ratio_min, Ratio_max)) {
			contours.erase(Begin + i);
			i--;
		}
	}
	if (contours.empty()) return NO_DOCS;

	//*
	//Exlude insiders contours (and double)
	sort(contours.begin(), contours.end(), sortArea);
	Begin = contours.begin();
	for (int i = 0; i < int(contours.size() - 1); ++i) {
		const double Peri = arcLength(contours[i], true);
		if (length_min <= Peri && Peri <= length_max) {
			for (int j = i + 1; j < int(contours.size()); ++j) {
				if (inQuad(contours[i], GetCenter(contours[j]))) {
					contours.erase(Begin + j);
					j--;
				}
			}
		}
	}
	if (contours.empty()) return NO_DOCS;

	return NO_ERRORS;
}

bool Extract4Corners(vector<Point> &contour, const double length_min, const double length_max)
{
	const int Nb_points = int(contour.size());
	if (Nb_points < 4) return false;
	if (Nb_points > 4) {
		int Ids[4] = {0, 0, 0, 0};
		set<int> Corners_id;			//Use set to avoid duplication
		Point Center = GetCenter(contour);

		//***** Get Diagonal (Maximize Distance) ****
		for (int k = 2; k < 4; ++k) {
			int Dist_max = 0;
			int Id_farest = 0;
			for (int i = 0; i < Nb_points; ++i) {
				const int dist = SquaredDist(contour[i], Center);
				if (dist > Dist_max) {
					Dist_max = dist;
					Id_farest = i;
				}
			}
			Corners_id.insert(Id_farest);
			Ids[k] = Id_farest;
			Center = contour[Id_farest];
		}

		//***** Find Other Points (Maximize Area) ****
		double Areas_max[2] = {0.0, 0.0};
		const Point AB = contour[Ids[3]] - contour[Ids[2]];
		for (int i = 0; i < Nb_points; ++i) {
			const Point AC = contour[i] - contour[Ids[2]],
						BC = contour[i] - contour[Ids[3]];
			const int d = AB.x * AC.y - AB.y * AC.x;
			//if (d = 0) C is on Diagonal
			if (d != 0) {
				const int side = d > 0 ? 0 : 1;
				const double Dist_AB = Dist(AB),
							 Dist_AC = Dist(AC),
							 Dist_BC = Dist(BC),
							 peri_2 = (Dist_AB + Dist_AC + Dist_BC) / 2;
				// False area based on Heron's formula without square root (maybe avoid on distance too...)
				const double area = peri_2 * (peri_2 - Dist_AC) * (peri_2 - Dist_BC) * (peri_2 - Dist_AB);
				if (area > Areas_max[side]) {
					Areas_max[side] = area;
					Ids[side] = i;
				}
			}
		}
		Corners_id.insert(Ids[0]);
		Corners_id.insert(Ids[1]);

		if (Corners_id.size() != 4) return false;
		vector<Point> Res;
		Res.reserve(4);
		Res.push_back(contour[Ids[2]]);	// First Point Of Diag
		Res.push_back(contour[Ids[0]]);	// Left Point
		Res.push_back(contour[Ids[3]]);	// Second Point of Diag
		Res.push_back(contour[Ids[1]]);	// Right Point
		const double Peri2 = arcLength(Res, true);
		if (!inRange(Peri2, length_min, length_max)) return false;
		contour = Res;
	}
	return true;
}

bool inQuad(const vector<Point> &quad, const Point &point)
{
	int cross[2] = {0, 0};
	for (int i = 0; i < 3; ++i) {
		const int sign = CrossProduct(quad[i + 1] - quad[i], quad[i] - point) >= 0 ? 1 : 0;
		cross[sign]++;
	}
	const int sign = CrossProduct(quad[0] - quad[3], quad[3] - point) >= 0 ? 1 : 0;
	cross[sign]++;

	return cross[0] == 0 || cross[1] == 0;
}

int UnDistord(const Mat &src, vector<Point> &contour, Mat &dst)
{
	vector<Point> contour2;
	//Ordering to Top-Left Top-Right Bottom-Right Bottom-Left and get max width and height
	sortContourPoints(contour, contour2);
	const float W = float(sqrt(MAX(SquaredDist(contour2[0], contour2[1]), SquaredDist(contour2[2], contour2[3])))),
				H = float(sqrt(MAX(SquaredDist(contour2[1], contour2[2]), SquaredDist(contour2[3], contour2[0]))));
	//Only Murphy can have this exception I can't reproduce him
	if (W == 0 || H == 0) return INVALID_DOC;

	const Point2f R1[4] = {	Point2f(contour2[0]), Point2f(contour2[1]),
							Point2f(contour2[2]), Point2f(contour2[3])},
				  R2[4] = {	Point2f(0.0f, 0.0f), Point2f(W - 1.0f, 0.0f),
				  			Point2f(W - 1.0f, H - 1.0f), Point2f(0.0f, H - 1.0f)};

	const Mat M = getPerspectiveTransform(R1, R2);

	warpPerspective(src, dst, M, Size(int(W), int(H)));
	return NO_ERRORS;
}
//****************************

//********************************
//********** Unity Link **********
DLL_EXPORT DocsDetection(Color32 *image, uint width, uint height,
						 Color32 background, uint *outDocsCount, int *outDocsPoints)
{
	Mat src;
	Scalar Background;
	vector<vector<Point>> Contours;
	//Conversion
	int ErrCode = UnityToOpenCVMat(image, height, width, src);
	if (ErrCode != NO_ERRORS) return ErrCode;
	//Detection
	Color32ToScalar(background, Background);
	ErrCode = DocsDetection(src, Background, Contours);
	if (ErrCode != NO_ERRORS) return ErrCode;
	//Conversion
	vector<Vec8i> Docs;
	ContoursToDocs(Contours, Docs);
	*outDocsCount = uint(Docs.size());
	return DocsToUnity(Docs, outDocsPoints, *outDocsCount);
}

DLL_EXPORT DocExtraction(Color32 *image, uint width, uint height, Color32 background, int *outDocPoints)
{
	Mat src, dst;
	Scalar Background;
	vector<Point> Contour;
	//Conversion
	int ErrCode = UnityToOpenCVMat(image, height, width, src);
	if (ErrCode != NO_ERRORS) return ErrCode;
	Color32ToScalar(background, Background);
	//Extraction
	ErrCode = DocExtraction(src, Background, Contour, dst);
	if (ErrCode != NO_ERRORS) return ErrCode;
	//Conversion

	return NO_ERRORS;
}

DLL_EXPORT SimpleDocsDetection(Color32 *image, uint width, uint height,
							   byte *result, uint maxDocsCount, uint *outDocsCount, int *outDocsPoints)
{
	Mat src, edgeDetect;
	UnityToOpenCVMat(image, height, width, src);
	BinaryEdgeDetector(src, edgeDetect);

	vector<vector<Point>> contours;
	findContours(edgeDetect, contours, CV_RETR_TREE, CV_CHAIN_APPROX_SIMPLE);

	// Sorting the contour depending on their area
	std::sort(contours.begin(), contours.end(), sortArea);

	int index = 0;
	vector<int> viableContoursIndexes;
	vector<Point> approx;
	vector<Vec8i> docsPoints;
	for (const vector<Point> &contour : contours) {
		const double peri = arcLength(contour, true);
		approxPolyDP(contour, approx, 0.02 * peri, true);
		if (approx.size() == 4) {
			// Sorting points
			std::sort(approx.begin(), approx.end(), sortPointsX);
			std::sort(approx.begin() + 1, approx.end() - 1, sortPointsY);

			Vec8i points = {
				approx.at(0).x, int(height) - approx.at(0).y,
				approx.at(1).x, int(height) - approx.at(1).y,
				approx.at(3).x, int(height) - approx.at(3).y,
				approx.at(2).x, int(height) - approx.at(2).y
			};

			docsPoints.emplace_back(points);

			if (docsPoints.size() == maxDocsCount) {
				break;
			}

			viableContoursIndexes.push_back(index);
		}
		index++;
	}

	for (int viableContoursIndex : viableContoursIndexes) {
		drawContours(src, contours, viableContoursIndex, Scalar(0, 255, 0), 2);
	}

	*outDocsCount = uint(docsPoints.size());
	OpenCVMatToUnity(src, result);
	return DocsToUnity(docsPoints, outDocsPoints, *outDocsCount);
}
//********************************

//*****************************
//********** Methods **********
int DocsDetection(const Mat &src, const Scalar &background, vector<vector<Point>> &contours)
{
	Mat Im_binary;
	//Binarisation
	const int ErrCode = Binarisation(src, Im_binary, background, COLOR_RANGE);
	if (ErrCode != NO_ERRORS) return ErrCode;
	//FindContour
	const int perimeter = 2 * (src.cols + src.rows);
	const double Length_min = RATIO_LENGTH_MIN * perimeter,
				 Length_max = RATIO_LENGTH_MAX * perimeter;
	return DocsContours(Im_binary, contours, Length_min, Length_max);
}

int DocExtraction(const Mat &src, const Scalar &background, std::vector<Point> &contour, Mat &dst)
{
	vector<vector<Point>> Contours;
	int ErrCode = DocsDetection(src, background, Contours);
	if (ErrCode != NO_ERRORS) return ErrCode;
	contour = GetCenterestContour(Contours, Point(src.cols / 2, src.rows / 2));
	ErrCode = UnDistord(src, contour, dst);
	return ErrCode;
}

int FeaturesExtraction(const cv::Mat &src)
{
	return NO_ERRORS;
}

int CompareDocs(const cv::Mat &im1, const cv::Mat &im2, double &similarity)
{
	return NO_ERRORS;
}

int CompareFeatures(double &similarity)
{
	//use findHomography between 2 features and check number of 0 and 1 to have similarity ????
	//findHomography(srcPoints, dstPoints, RANSAC, status);
	return NO_ERRORS;
}

//*****************************

//******************************
//********** Computes **********
//******************************
int BinaryEdgeDetector(const Mat &src, Mat &dst, const int min_tresh, const int max_tresh, const int aperture)
{
	if (src.empty()) return EMPTY_MAT;
	if (src.type() != CV_8UC1 && src.type() != CV_8UC3) return TYPE_MAT;
	Mat gray;
	if (src.type() == CV_8UC3) {
		// Convert image to gray and blur it
		cvtColor(src, gray, CV_BGR2GRAY);
	} else gray = src.clone();

	blur(gray, gray, Size(3, 3));
	Canny(gray, dst, min_tresh, max_tresh, aperture);
	return NO_ERRORS;
}
