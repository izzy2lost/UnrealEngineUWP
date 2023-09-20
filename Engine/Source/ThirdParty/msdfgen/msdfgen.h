
/*
 * MULTI-CHANNEL SIGNED DISTANCE FIELD GENERATOR
 * ---------------------------------------------
 * A utility by Viktor Chlumsky, (c) 2014 - 2023
 * https://github.com/Chlumsky/msdfgen
 * Published under the MIT license
 *
 * The technique used to generate multi-channel distance fields in this code
 * was developed by Viktor Chlumsky in 2014 for his master's thesis,
 * "Shape Decomposition for Multi-Channel Distance Fields". It provides improved
 * quality of sharp corners in glyphs and other 2D shapes compared to monochrome
 * distance fields. To reconstruct an image of the shape, apply the median of three
 * operation on the triplet of sampled signed distance values.
 *
 */

#pragma once

#define MSDFGEN_USE_CPP11
#define MSDFGEN_USE_FREETYPE
#define MSDFGEN_DISABLE_VARIABLE_FONTS

#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <vector>

namespace msdfgen {

/// Returns the smaller of the arguments.
template <typename T>
inline T min(T a, T b) {
    return b < a ? b : a;
}

/// Returns the larger of the arguments.
template <typename T>
inline T max(T a, T b) {
    return a < b ? b : a;
}

/// Returns the middle out of three values
template <typename T>
inline T median(T a, T b, T c) {
    return max(min(a, b), min(max(a, b), c));
}

/// Returns the weighted average of a and b.
template <typename T, typename S>
inline T mix(T a, T b, S weight) {
    return T((S(1)-weight)*a+weight*b);
}

/// Clamps the number to the interval from 0 to 1.
template <typename T>
inline T clamp(T n) {
    return n >= T(0) && n <= T(1) ? n : T(n > T(0));
}

/// Clamps the number to the interval from 0 to b.
template <typename T>
inline T clamp(T n, T b) {
    return n >= T(0) && n <= b ? n : T(n > T(0))*b;
}

/// Clamps the number to the interval from a to b.
template <typename T>
inline T clamp(T n, T a, T b) {
    return n >= a && n <= b ? n : n < a ? a : b;
}

/// Returns 1 for positive values, -1 for negative values, and 0 for zero.
template <typename T>
inline int sign(T n) {
    return (T(0) < n)-(n < T(0));
}

/// Returns 1 for non-negative values and -1 for negative values.
template <typename T>
inline int nonZeroSign(T n) {
    return 2*(n > T(0))-1;
}

// ax^2 + bx + c = 0
int solveQuadratic(double x[2], double a, double b, double c);

// ax^3 + bx^2 + cx + d = 0
int solveCubic(double x[3], double a, double b, double c, double d);

/**
* A 2-dimensional euclidean vector with double precision.
* Implementation based on the Vector2 template from Artery Engine.
* @author Viktor Chlumsky
*/
struct Vector2 {

    double x, y;

    Vector2(double val = 0);
    Vector2(double x, double y);
    /// Sets the vector to zero.
    void reset();
    /// Sets individual elements of the vector.
    void set(double x, double y);
    /// Returns the vector's length.
    double length() const;
    /// Returns the angle of the vector in radians (atan2).
    double direction() const;
    /// Returns the normalized vector - one that has the same direction but unit length.
    Vector2 normalize(bool allowZero = false) const;
    /// Returns a vector with the same length that is orthogonal to this one.
    Vector2 getOrthogonal(bool polarity = true) const;
    /// Returns a vector with unit length that is orthogonal to this one.
    Vector2 getOrthonormal(bool polarity = true, bool allowZero = false) const;
    /// Returns a vector projected along this one.
    Vector2 project(const Vector2 &vector, bool positive = false) const;
    operator const void *() const;
    bool operator!() const;
    bool operator==(const Vector2 &other) const;
    bool operator!=(const Vector2 &other) const;
    Vector2 operator+() const;
    Vector2 operator-() const;
    Vector2 operator+(const Vector2 &other) const;
    Vector2 operator-(const Vector2 &other) const;
    Vector2 operator*(const Vector2 &other) const;
    Vector2 operator/(const Vector2 &other) const;
    Vector2 operator*(double value) const;
    Vector2 operator/(double value) const;
    Vector2 & operator+=(const Vector2 &other);
    Vector2 & operator-=(const Vector2 &other);
    Vector2 & operator*=(const Vector2 &other);
    Vector2 & operator/=(const Vector2 &other);
    Vector2 & operator*=(double value);
    Vector2 & operator/=(double value);
    /// Dot product of two vectors.
    friend double dotProduct(const Vector2 &a, const Vector2 &b);
    /// A special version of the cross product for 2D vectors (returns scalar value).
    friend double crossProduct(const Vector2 &a, const Vector2 &b);
    friend Vector2 operator*(double value, const Vector2 &vector);
    friend Vector2 operator/(double value, const Vector2 &vector);

};

/// A vector may also represent a point, which shall be differentiated semantically using the alias Point2.
typedef Vector2 Point2;

typedef unsigned char byte;

inline byte pixelFloatToByte(float x) {
    return byte(clamp(256.f*x, 255.f));
}

inline float pixelByteToFloat(byte x) {
    return 1.f/255.f*float(x);
}

typedef unsigned char byte;

/// Reference to a 2D image bitmap or a buffer acting as one. Pixel storage not owned or managed by the object.
template <typename T, int N = 1>
struct BitmapRef {

    T *pixels;
    int width, height;

    inline BitmapRef() : pixels(NULL), width(0), height(0) { }
    inline BitmapRef(T *pixels, int width, int height) : pixels(pixels), width(width), height(height) { }

    inline T * operator()(int x, int y) const {
        return pixels+N*(width*y+x);
    }

};

/// Constant reference to a 2D image bitmap or a buffer acting as one. Pixel storage not owned or managed by the object.
template <typename T, int N = 1>
struct BitmapConstRef {

    const T *pixels;
    int width, height;

    inline BitmapConstRef() : pixels(NULL), width(0), height(0) { }
    inline BitmapConstRef(const T *pixels, int width, int height) : pixels(pixels), width(width), height(height) { }
    inline BitmapConstRef(const BitmapRef<T, N> &orig) : pixels(orig.pixels), width(orig.width), height(orig.height) { }

    inline const T * operator()(int x, int y) const {
        return pixels+N*(width*y+x);
    }

};

/// A 2D image bitmap with N channels of type T. Pixel memory is managed by the class.
template <typename T, int N = 1>
class Bitmap {

public:
    Bitmap();
    Bitmap(int width, int height);
    Bitmap(const BitmapConstRef<T, N> &orig);
    Bitmap(const Bitmap<T, N> &orig);
#ifdef MSDFGEN_USE_CPP11
    Bitmap(Bitmap<T, N> &&orig);
#endif
    ~Bitmap();
    Bitmap<T, N> & operator=(const BitmapConstRef<T, N> &orig);
    Bitmap<T, N> & operator=(const Bitmap<T, N> &orig);
#ifdef MSDFGEN_USE_CPP11
    Bitmap<T, N> & operator=(Bitmap<T, N> &&orig);
#endif
    /// Bitmap width in pixels.
    int width() const;
    /// Bitmap height in pixels.
    int height() const;
    T * operator()(int x, int y);
    const T * operator()(int x, int y) const;
#ifdef MSDFGEN_USE_CPP11
    explicit operator T *();
    explicit operator const T *() const;
#else
    operator T *();
    operator const T *() const;
#endif
    operator BitmapRef<T, N>();
    operator BitmapConstRef<T, N>() const;

private:
    T *pixels;
    int w, h;

};

template <typename T, int N>
Bitmap<T, N>::Bitmap() : pixels(NULL), w(0), h(0) { }

template <typename T, int N>
Bitmap<T, N>::Bitmap(int width, int height) : w(width), h(height) {
    pixels = new T[N*w*h];
}

template <typename T, int N>
Bitmap<T, N>::Bitmap(const BitmapConstRef<T, N> &orig) : w(orig.width), h(orig.height) {
    pixels = new T[N*w*h];
    memcpy(pixels, orig.pixels, sizeof(T)*N*w*h);
}

template <typename T, int N>
Bitmap<T, N>::Bitmap(const Bitmap<T, N> &orig) : w(orig.w), h(orig.h) {
    pixels = new T[N*w*h];
    memcpy(pixels, orig.pixels, sizeof(T)*N*w*h);
}

#ifdef MSDFGEN_USE_CPP11
template <typename T, int N>
Bitmap<T, N>::Bitmap(Bitmap<T, N> &&orig) : pixels(orig.pixels), w(orig.w), h(orig.h) {
    orig.pixels = NULL;
    orig.w = 0, orig.h = 0;
}
#endif

template <typename T, int N>
Bitmap<T, N>::~Bitmap() {
    delete [] pixels;
}

template <typename T, int N>
Bitmap<T, N> & Bitmap<T, N>::operator=(const BitmapConstRef<T, N> &orig) {
    if (pixels != orig.pixels) {
        delete [] pixels;
        w = orig.width, h = orig.height;
        pixels = new T[N*w*h];
        memcpy(pixels, orig.pixels, sizeof(T)*N*w*h);
    }
    return *this;
}

template <typename T, int N>
Bitmap<T, N> & Bitmap<T, N>::operator=(const Bitmap<T, N> &orig) {
    if (this != &orig) {
        delete [] pixels;
        w = orig.w, h = orig.h;
        pixels = new T[N*w*h];
        memcpy(pixels, orig.pixels, sizeof(T)*N*w*h);
    }
    return *this;
}

#ifdef MSDFGEN_USE_CPP11
template <typename T, int N>
Bitmap<T, N> & Bitmap<T, N>::operator=(Bitmap<T, N> &&orig) {
    if (this != &orig) {
        delete [] pixels;
        pixels = orig.pixels;
        w = orig.w, h = orig.h;
        orig.pixels = NULL;
    }
    return *this;
}
#endif

template <typename T, int N>
int Bitmap<T, N>::width() const {
    return w;
}

template <typename T, int N>
int Bitmap<T, N>::height() const {
    return h;
}

template <typename T, int N>
T * Bitmap<T, N>::operator()(int x, int y) {
    return pixels+N*(w*y+x);
}

template <typename T, int N>
const T * Bitmap<T, N>::operator()(int x, int y) const {
    return pixels+N*(w*y+x);
}

template <typename T, int N>
Bitmap<T, N>::operator T *() {
    return pixels;
}

template <typename T, int N>
Bitmap<T, N>::operator const T *() const {
    return pixels;
}

template <typename T, int N>
Bitmap<T, N>::operator BitmapRef<T, N>() {
    return BitmapRef<T, N>(pixels, w, h);
}

template <typename T, int N>
Bitmap<T, N>::operator BitmapConstRef<T, N>() const {
    return BitmapConstRef<T, N>(pixels, w, h);
}

/// A transformation from shape coordinates to pixel coordinates.
class Projection {

public:
    Projection();
    Projection(const Vector2 &scale, const Vector2 &translate);
    /// Converts the shape coordinate to pixel coordinate.
    Point2 project(const Point2 &coord) const;
    /// Converts the pixel coordinate to shape coordinate.
    Point2 unproject(const Point2 &coord) const;
    /// Converts the vector to pixel coordinate space.
    Vector2 projectVector(const Vector2 &vector) const;
    /// Converts the vector from pixel coordinate space.
    Vector2 unprojectVector(const Vector2 &vector) const;
    /// Converts the X-coordinate from shape to pixel coordinate space.
    double projectX(double x) const;
    /// Converts the Y-coordinate from shape to pixel coordinate space.
    double projectY(double y) const;
    /// Converts the X-coordinate from pixel to shape coordinate space.
    double unprojectX(double x) const;
    /// Converts the Y-coordinate from pixel to shape coordinate space.
    double unprojectY(double y) const;

private:
    Vector2 scale;
    Vector2 translate;

};

/// Represents a signed distance and alignment, which together can be compared to uniquely determine the closest edge segment.
class SignedDistance {

public:
    double distance;
    double dot;

    SignedDistance();
    SignedDistance(double dist, double d);

    friend bool operator<(SignedDistance a, SignedDistance b);
    friend bool operator>(SignedDistance a, SignedDistance b);
    friend bool operator<=(SignedDistance a, SignedDistance b);
    friend bool operator>=(SignedDistance a, SignedDistance b);

};

/// Fill rule dictates how intersection total is interpreted during rasterization.
enum FillRule {
    FILL_NONZERO,
    FILL_ODD, // "even-odd"
    FILL_POSITIVE,
    FILL_NEGATIVE
};

/// Resolves the number of intersection into a binary fill value based on fill rule.
bool interpretFillRule(int intersections, FillRule fillRule);

/// Represents a horizontal scanline intersecting a shape.
class Scanline {

public:
    /// An intersection with the scanline.
    struct Intersection {
        /// X coordinate.
        double x;
        /// Normalized Y direction of the oriented edge at the point of intersection.
        int direction;
    };

    static double overlap(const Scanline &a, const Scanline &b, double xFrom, double xTo, FillRule fillRule);

    Scanline();
    /// Populates the intersection list.
    void setIntersections(const std::vector<Intersection> &intersections);
#ifdef MSDFGEN_USE_CPP11
    void setIntersections(std::vector<Intersection> &&intersections);
#endif
    /// Returns the number of intersections left of x.
    int countIntersections(double x) const;
    /// Returns the total sign of intersections left of x.
    int sumIntersections(double x) const;
    /// Decides whether the scanline is filled at x based on fill rule.
    bool filled(double x, FillRule fillRule) const;

private:
    std::vector<Intersection> intersections;
    mutable int lastIndex;

    void preprocess();
    int moveTo(double x) const;

};

/// Edge color specifies which color channels an edge belongs to.
enum EdgeColor {
    BLACK = 0,
    RED = 1,
    GREEN = 2,
    YELLOW = 3,
    BLUE = 4,
    MAGENTA = 5,
    CYAN = 6,
    WHITE = 7
};

// Parameters for iterative search of closest point on a cubic Bezier curve. Increase for higher precision.
#define MSDFGEN_CUBIC_SEARCH_STARTS 4
#define MSDFGEN_CUBIC_SEARCH_STEPS 4

/// An abstract edge segment.
class EdgeSegment {

public:
    EdgeColor color;

    EdgeSegment(EdgeColor edgeColor = WHITE) : color(edgeColor) { }
    virtual ~EdgeSegment() { }
    /// Creates a copy of the edge segment.
    virtual EdgeSegment * clone() const = 0;
    /// Returns the numeric code of the edge segment's type.
    virtual int type() const = 0;
    /// Returns the array of control points.
    virtual const Point2 * controlPoints() const = 0;
    /// Returns the point on the edge specified by the parameter (between 0 and 1).
    virtual Point2 point(double param) const = 0;
    /// Returns the direction the edge has at the point specified by the parameter.
    virtual Vector2 direction(double param) const = 0;
    /// Returns the change of direction (second derivative) at the point specified by the parameter.
    virtual Vector2 directionChange(double param) const = 0;
    /// Returns the minimum signed distance between origin and the edge.
    virtual SignedDistance signedDistance(Point2 origin, double &param) const = 0;
    /// Converts a previously retrieved signed distance from origin to pseudo-distance.
    virtual void distanceToPseudoDistance(SignedDistance &distance, Point2 origin, double param) const;
    /// Outputs a list of (at most three) intersections (their X coordinates) with an infinite horizontal scanline at y and returns how many there are.
    virtual int scanlineIntersections(double x[3], int dy[3], double y) const = 0;
    /// Adjusts the bounding box to fit the edge segment.
    virtual void bound(double &l, double &b, double &r, double &t) const = 0;

    /// Reverses the edge (swaps its start point and end point).
    virtual void reverse() = 0;
    /// Moves the start point of the edge segment.
    virtual void moveStartPoint(Point2 to) = 0;
    /// Moves the end point of the edge segment.
    virtual void moveEndPoint(Point2 to) = 0;
    /// Splits the edge segments into thirds which together represent the original edge.
    virtual void splitInThirds(EdgeSegment *&part1, EdgeSegment *&part2, EdgeSegment *&part3) const = 0;

};

/// A line segment.
class LinearSegment : public EdgeSegment {

public:
    enum EdgeType {
        EDGE_TYPE = 1
    };

    Point2 p[2];

    LinearSegment(Point2 p0, Point2 p1, EdgeColor edgeColor = WHITE);
    LinearSegment * clone() const;
    int type() const;
    const Point2 * controlPoints() const;
    Point2 point(double param) const;
    Vector2 direction(double param) const;
    Vector2 directionChange(double param) const;
    double length() const;
    SignedDistance signedDistance(Point2 origin, double &param) const;
    int scanlineIntersections(double x[3], int dy[3], double y) const;
    void bound(double &l, double &b, double &r, double &t) const;

    void reverse();
    void moveStartPoint(Point2 to);
    void moveEndPoint(Point2 to);
    void splitInThirds(EdgeSegment *&part1, EdgeSegment *&part2, EdgeSegment *&part3) const;

};

/// A quadratic Bezier curve.
class QuadraticSegment : public EdgeSegment {

public:
    enum EdgeType {
        EDGE_TYPE = 2
    };

    Point2 p[3];

    QuadraticSegment(Point2 p0, Point2 p1, Point2 p2, EdgeColor edgeColor = WHITE);
    QuadraticSegment * clone() const;
    int type() const;
    const Point2 * controlPoints() const;
    Point2 point(double param) const;
    Vector2 direction(double param) const;
    Vector2 directionChange(double param) const;
    double length() const;
    SignedDistance signedDistance(Point2 origin, double &param) const;
    int scanlineIntersections(double x[3], int dy[3], double y) const;
    void bound(double &l, double &b, double &r, double &t) const;

    void reverse();
    void moveStartPoint(Point2 to);
    void moveEndPoint(Point2 to);
    void splitInThirds(EdgeSegment *&part1, EdgeSegment *&part2, EdgeSegment *&part3) const;

    EdgeSegment * convertToCubic() const;

};

/// A cubic Bezier curve.
class CubicSegment : public EdgeSegment {

public:
    enum EdgeType {
        EDGE_TYPE = 3
    };

    Point2 p[4];

    CubicSegment(Point2 p0, Point2 p1, Point2 p2, Point2 p3, EdgeColor edgeColor = WHITE);
    CubicSegment * clone() const;
    int type() const;
    const Point2 * controlPoints() const;
    Point2 point(double param) const;
    Vector2 direction(double param) const;
    Vector2 directionChange(double param) const;
    SignedDistance signedDistance(Point2 origin, double &param) const;
    int scanlineIntersections(double x[3], int dy[3], double y) const;
    void bound(double &l, double &b, double &r, double &t) const;

    void reverse();
    void moveStartPoint(Point2 to);
    void moveEndPoint(Point2 to);
    void splitInThirds(EdgeSegment *&part1, EdgeSegment *&part2, EdgeSegment *&part3) const;

    void deconverge(int param, double amount);

};

/// Container for a single edge of dynamic type.
class EdgeHolder {

public:
    /// Swaps the edges held by a and b.
    static void swap(EdgeHolder &a, EdgeHolder &b);

    EdgeHolder();
    EdgeHolder(EdgeSegment *segment);
    EdgeHolder(Point2 p0, Point2 p1, EdgeColor edgeColor = WHITE);
    EdgeHolder(Point2 p0, Point2 p1, Point2 p2, EdgeColor edgeColor = WHITE);
    EdgeHolder(Point2 p0, Point2 p1, Point2 p2, Point2 p3, EdgeColor edgeColor = WHITE);
    EdgeHolder(const EdgeHolder &orig);
#ifdef MSDFGEN_USE_CPP11
    EdgeHolder(EdgeHolder &&orig);
#endif
    ~EdgeHolder();
    EdgeHolder & operator=(const EdgeHolder &orig);
#ifdef MSDFGEN_USE_CPP11
    EdgeHolder & operator=(EdgeHolder &&orig);
#endif
    EdgeSegment & operator*();
    const EdgeSegment & operator*() const;
    EdgeSegment * operator->();
    const EdgeSegment * operator->() const;
    operator EdgeSegment *();
    operator const EdgeSegment *() const;

private:
    EdgeSegment *edgeSegment;

};

/// A single closed contour of a shape.
class Contour {

public:
    /// The sequence of edges that make up the contour.
    std::vector<EdgeHolder> edges;

    /// Adds an edge to the contour.
    void addEdge(const EdgeHolder &edge);
#ifdef MSDFGEN_USE_CPP11
    void addEdge(EdgeHolder &&edge);
#endif
    /// Creates a new edge in the contour and returns its reference.
    EdgeHolder & addEdge();
    /// Adjusts the bounding box to fit the contour.
    void bound(double &l, double &b, double &r, double &t) const;
    /// Adjusts the bounding box to fit the contour border's mitered corners.
    void boundMiters(double &l, double &b, double &r, double &t, double border, double miterLimit, int polarity) const;
    /// Computes the winding of the contour. Returns 1 if positive, -1 if negative.
    int winding() const;
    /// Reverses the sequence of edges on the contour.
    void reverse();

};

// Threshold of the dot product of adjacent edge directions to be considered convergent.
#define MSDFGEN_CORNER_DOT_EPSILON .000001
// The proportional amount by which a curve's control point will be adjusted to eliminate convergent corners.
#define MSDFGEN_DECONVERGENCE_FACTOR .000001

/// Vector shape representation.
class Shape {

public:
    struct Bounds {
        double l, b, r, t;
    };

    /// The list of contours the shape consists of.
    std::vector<Contour> contours;
    /// Specifies whether the shape uses bottom-to-top (false) or top-to-bottom (true) Y coordinates.
    bool inverseYAxis;

    Shape();
    /// Adds a contour.
    void addContour(const Contour &contour);
#ifdef MSDFGEN_USE_CPP11
    void addContour(Contour &&contour);
#endif
    /// Adds a blank contour and returns its reference.
    Contour & addContour();
    /// Normalizes the shape geometry for distance field generation.
    void normalize();
    /// Performs basic checks to determine if the object represents a valid shape.
    bool validate() const;
    /// Adjusts the bounding box to fit the shape.
    void bound(double &l, double &b, double &r, double &t) const;
    /// Adjusts the bounding box to fit the shape border's mitered corners.
    void boundMiters(double &l, double &b, double &r, double &t, double border, double miterLimit, int polarity) const;
    /// Computes the minimum bounding box that fits the shape, optionally with a (mitered) border.
    Bounds getBounds(double border = 0, double miterLimit = 0, int polarity = 0) const;
    /// Outputs the scanline that intersects the shape at y.
    void scanline(Scanline &line, double y) const;
    /// Returns the total number of edge segments
    int edgeCount() const;
    /// Assumes its contours are unoriented (even-odd fill rule). Attempts to orient them to conform to the non-zero winding rule.
    void orientContours();

};

template <typename T, int N>
static void interpolate(T *output, const BitmapConstRef<T, N> &bitmap, Point2 pos) {
    pos -= .5;
    int l = (int) floor(pos.x);
    int b = (int) floor(pos.y);
    int r = l+1;
    int t = b+1;
    double lr = pos.x-l;
    double bt = pos.y-b;
    l = clamp(l, bitmap.width-1), r = clamp(r, bitmap.width-1);
    b = clamp(b, bitmap.height-1), t = clamp(t, bitmap.height-1);
    for (int i = 0; i < N; ++i)
        output[i] = mix(mix(bitmap(l, b)[i], bitmap(r, b)[i], lr), mix(bitmap(l, t)[i], bitmap(r, t)[i], lr), bt);
}

}

#define MSDFGEN_EDGE_LENGTH_PRECISION 4

namespace msdfgen {

/** Assigns colors to edges of the shape in accordance to the multi-channel distance field technique.
 *  May split some edges if necessary.
 *  angleThreshold specifies the maximum angle (in radians) to be considered a corner, for example 3 (~172 degrees).
 *  Values below 1/2 PI will be treated as the external angle.
 */
void edgeColoringSimple(Shape &shape, double angleThreshold, unsigned long long seed = 0);

/** The alternative "ink trap" coloring strategy is designed for better results with typefaces
 *  that use ink traps as a design feature. It guarantees that even if all edges that are shorter than
 *  both their neighboring edges are removed, the coloring remains consistent with the established rules.
 */
void edgeColoringInkTrap(Shape &shape, double angleThreshold, unsigned long long seed = 0);

/** The alternative coloring by distance tries to use different colors for edges that are close together.
 *  This should theoretically be the best strategy on average. However, since it needs to compute the distance
 *  between all pairs of edges, and perform a graph optimization task, it is much slower than the rest.
 */
void edgeColoringByDistance(Shape &shape, double angleThreshold, unsigned long long seed = 0);

struct MultiDistance {
    double r, g, b;
};
struct MultiAndTrueDistance : MultiDistance {
    double a;
};

/// Selects the nearest edge by its true distance.
class TrueDistanceSelector {

public:
    typedef double DistanceType;

    struct EdgeCache {
        Point2 point;
        double absDistance;

        EdgeCache();
    };

    void reset(const Point2 &p);
    void addEdge(EdgeCache &cache, const EdgeSegment *prevEdge, const EdgeSegment *edge, const EdgeSegment *nextEdge);
    void merge(const TrueDistanceSelector &other);
    DistanceType distance() const;

private:
    Point2 p;
    SignedDistance minDistance;

};

class PseudoDistanceSelectorBase {

public:
    struct EdgeCache {
        Point2 point;
        double absDistance;
        double aDomainDistance, bDomainDistance;
        double aPseudoDistance, bPseudoDistance;

        EdgeCache();
    };

    static bool getPseudoDistance(double &distance, const Vector2 &ep, const Vector2 &edgeDir);

    PseudoDistanceSelectorBase();
    void reset(double delta);
    bool isEdgeRelevant(const EdgeCache &cache, const EdgeSegment *edge, const Point2 &p) const;
    void addEdgeTrueDistance(const EdgeSegment *edge, const SignedDistance &distance, double param);
    void addEdgePseudoDistance(double distance);
    void merge(const PseudoDistanceSelectorBase &other);
    double computeDistance(const Point2 &p) const;
    SignedDistance trueDistance() const;

private:
    SignedDistance minTrueDistance;
    double minNegativePseudoDistance;
    double minPositivePseudoDistance;
    const EdgeSegment *nearEdge;
    double nearEdgeParam;

};

/// Selects the nearest edge by its pseudo-distance.
class PseudoDistanceSelector : public PseudoDistanceSelectorBase {

public:
    typedef double DistanceType;

    void reset(const Point2 &p);
    void addEdge(EdgeCache &cache, const EdgeSegment *prevEdge, const EdgeSegment *edge, const EdgeSegment *nextEdge);
    DistanceType distance() const;

private:
    Point2 p;

};

/// Selects the nearest edge for each of the three channels by its pseudo-distance.
class MultiDistanceSelector {

public:
    typedef MultiDistance DistanceType;
    typedef PseudoDistanceSelectorBase::EdgeCache EdgeCache;

    void reset(const Point2 &p);
    void addEdge(EdgeCache &cache, const EdgeSegment *prevEdge, const EdgeSegment *edge, const EdgeSegment *nextEdge);
    void merge(const MultiDistanceSelector &other);
    DistanceType distance() const;
    SignedDistance trueDistance() const;

private:
    Point2 p;
    PseudoDistanceSelectorBase r, g, b;

};

/// Selects the nearest edge for each of the three color channels by its pseudo-distance and by true distance for the alpha channel.
class MultiAndTrueDistanceSelector : public MultiDistanceSelector {

public:
    typedef MultiAndTrueDistance DistanceType;

    DistanceType distance() const;

};

/// Simply selects the nearest contour.
template <class EdgeSelector>
class SimpleContourCombiner {

public:
    typedef EdgeSelector EdgeSelectorType;
    typedef typename EdgeSelector::DistanceType DistanceType;

    explicit SimpleContourCombiner(const Shape &shape);
    void reset(const Point2 &p);
    EdgeSelector & edgeSelector(int i);
    DistanceType distance() const;

private:
    EdgeSelector shapeEdgeSelector;

};

/// Selects the nearest contour that actually forms a border between filled and unfilled area.
template <class EdgeSelector>
class OverlappingContourCombiner {

public:
    typedef EdgeSelector EdgeSelectorType;
    typedef typename EdgeSelector::DistanceType DistanceType;

    explicit OverlappingContourCombiner(const Shape &shape);
    void reset(const Point2 &p);
    EdgeSelector & edgeSelector(int i);
    DistanceType distance() const;

private:
    Point2 p;
    std::vector<int> windings;
    std::vector<EdgeSelector> edgeSelectors;

};

/// Finds the distance between a point and a Shape. ContourCombiner dictates the distance metric and its data type.
template <class ContourCombiner>
class ShapeDistanceFinder {

public:
    typedef typename ContourCombiner::DistanceType DistanceType;

    // Passed shape object must persist until the distance finder is destroyed!
    explicit ShapeDistanceFinder(const Shape &shape);
    /// Finds the distance from origin. Not thread-safe! Is fastest when subsequent queries are close together.
    DistanceType distance(const Point2 &origin);

    /// Finds the distance between shape and origin. Does not allocate result cache used to optimize performance of multiple queries.
    static DistanceType oneShotDistance(const Shape &shape, const Point2 &origin);

private:
    const Shape &shape;
    ContourCombiner contourCombiner;
    std::vector<typename ContourCombiner::EdgeSelectorType::EdgeCache> shapeEdgeCache;

};

typedef ShapeDistanceFinder<SimpleContourCombiner<TrueDistanceSelector> > SimpleTrueShapeDistanceFinder;

template <class ContourCombiner>
ShapeDistanceFinder<ContourCombiner>::ShapeDistanceFinder(const Shape &shape) : shape(shape), contourCombiner(shape), shapeEdgeCache(shape.edgeCount()) { }

template <class ContourCombiner>
typename ShapeDistanceFinder<ContourCombiner>::DistanceType ShapeDistanceFinder<ContourCombiner>::distance(const Point2 &origin) {
    contourCombiner.reset(origin);
#ifdef MSDFGEN_USE_CPP11
    typename ContourCombiner::EdgeSelectorType::EdgeCache *edgeCache = shapeEdgeCache.data();
#else
    typename ContourCombiner::EdgeSelectorType::EdgeCache *edgeCache = shapeEdgeCache.empty() ? NULL : &shapeEdgeCache[0];
#endif

    for (std::vector<Contour>::const_iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour) {
        if (!contour->edges.empty()) {
            typename ContourCombiner::EdgeSelectorType &edgeSelector = contourCombiner.edgeSelector(int(contour-shape.contours.begin()));

            const EdgeSegment *prevEdge = contour->edges.size() >= 2 ? *(contour->edges.end()-2) : *contour->edges.begin();
            const EdgeSegment *curEdge = contour->edges.back();
            for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
                const EdgeSegment *nextEdge = *edge;
                edgeSelector.addEdge(*edgeCache++, prevEdge, curEdge, nextEdge);
                prevEdge = curEdge;
                curEdge = nextEdge;
            }
        }
    }

    return contourCombiner.distance();
}

template <class ContourCombiner>
typename ShapeDistanceFinder<ContourCombiner>::DistanceType ShapeDistanceFinder<ContourCombiner>::oneShotDistance(const Shape &shape, const Point2 &origin) {
    ContourCombiner contourCombiner(shape);
    contourCombiner.reset(origin);

    for (std::vector<Contour>::const_iterator contour = shape.contours.begin(); contour != shape.contours.end(); ++contour) {
        if (!contour->edges.empty()) {
            typename ContourCombiner::EdgeSelectorType &edgeSelector = contourCombiner.edgeSelector(int(contour-shape.contours.begin()));

            const EdgeSegment *prevEdge = contour->edges.size() >= 2 ? *(contour->edges.end()-2) : *contour->edges.begin();
            const EdgeSegment *curEdge = contour->edges.back();
            for (std::vector<EdgeHolder>::const_iterator edge = contour->edges.begin(); edge != contour->edges.end(); ++edge) {
                const EdgeSegment *nextEdge = *edge;
                typename ContourCombiner::EdgeSelectorType::EdgeCache dummy;
                edgeSelector.addEdge(dummy, prevEdge, curEdge, nextEdge);
                prevEdge = curEdge;
                curEdge = nextEdge;
            }
        }
    }

    return contourCombiner.distance();
}

}

#ifndef MSDFGEN_PUBLIC
#define MSDFGEN_PUBLIC // for DLL import/export
#endif

namespace msdfgen {

/// The configuration of the MSDF error correction pass.
struct ErrorCorrectionConfig {
    /// The default value of minDeviationRatio.
    static MSDFGEN_PUBLIC const double defaultMinDeviationRatio;
    /// The default value of minImproveRatio.
    static MSDFGEN_PUBLIC const double defaultMinImproveRatio;

    /// Mode of operation.
    enum Mode {
        /// Skips error correction pass.
        DISABLED,
        /// Corrects all discontinuities of the distance field regardless if edges are adversely affected.
        INDISCRIMINATE,
        /// Corrects artifacts at edges and other discontinuous distances only if it does not affect edges or corners.
        EDGE_PRIORITY,
        /// Only corrects artifacts at edges.
        EDGE_ONLY
    } mode;
    /// Configuration of whether to use an algorithm that computes the exact shape distance at the positions of suspected artifacts. This algorithm can be much slower.
    enum DistanceCheckMode {
        /// Never computes exact shape distance.
        DO_NOT_CHECK_DISTANCE,
        /// Only computes exact shape distance at edges. Provides a good balance between speed and precision.
        CHECK_DISTANCE_AT_EDGE,
        /// Computes and compares the exact shape distance for each suspected artifact.
        ALWAYS_CHECK_DISTANCE
    } distanceCheckMode;
    /// The minimum ratio between the actual and maximum expected distance delta to be considered an error.
    double minDeviationRatio;
    /// The minimum ratio between the pre-correction distance error and the post-correction distance error. Has no effect for DO_NOT_CHECK_DISTANCE.
    double minImproveRatio;
    /// An optional buffer to avoid dynamic allocation. Must have at least as many bytes as the MSDF has pixels.
    byte *buffer;

    inline explicit ErrorCorrectionConfig(Mode mode = EDGE_PRIORITY, DistanceCheckMode distanceCheckMode = CHECK_DISTANCE_AT_EDGE, double minDeviationRatio = defaultMinDeviationRatio, double minImproveRatio = defaultMinImproveRatio, byte *buffer = NULL) : mode(mode), distanceCheckMode(distanceCheckMode), minDeviationRatio(minDeviationRatio), minImproveRatio(minImproveRatio), buffer(buffer) { }
};

/// The configuration of the distance field generator algorithm.
struct GeneratorConfig {
    /// Specifies whether to use the version of the algorithm that supports overlapping contours with the same winding. May be set to false to improve performance when no such contours are present.
    bool overlapSupport;

    inline explicit GeneratorConfig(bool overlapSupport = true) : overlapSupport(overlapSupport) { }
};

/// The configuration of the multi-channel distance field generator algorithm.
struct MSDFGeneratorConfig : GeneratorConfig {
    /// Configuration of the error correction pass.
    ErrorCorrectionConfig errorCorrection;

    inline MSDFGeneratorConfig() { }
    inline explicit MSDFGeneratorConfig(bool overlapSupport, const ErrorCorrectionConfig &errorCorrection = ErrorCorrectionConfig()) : GeneratorConfig(overlapSupport), errorCorrection(errorCorrection) { }
};

/// Predicts potential artifacts caused by the interpolation of the MSDF and corrects them by converting nearby texels to single-channel.
void msdfErrorCorrection(const BitmapRef<float, 3> &sdf, const Shape &shape, const Projection &projection, double range, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());
void msdfErrorCorrection(const BitmapRef<float, 4> &sdf, const Shape &shape, const Projection &projection, double range, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());

/// Applies the simplified error correction to all discontiunous distances (INDISCRIMINATE mode). Does not need shape or translation.
void msdfFastDistanceErrorCorrection(const BitmapRef<float, 3> &sdf, const Projection &projection, double range, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);
void msdfFastDistanceErrorCorrection(const BitmapRef<float, 4> &sdf, const Projection &projection, double range, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);

/// Applies the simplified error correction to edges only (EDGE_ONLY mode). Does not need shape or translation.
void msdfFastEdgeErrorCorrection(const BitmapRef<float, 3> &sdf, const Projection &projection, double range, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);
void msdfFastEdgeErrorCorrection(const BitmapRef<float, 4> &sdf, const Projection &projection, double range, double minDeviationRatio = ErrorCorrectionConfig::defaultMinDeviationRatio);

/// The original version of the error correction algorithm.
void msdfErrorCorrection_legacy(const BitmapRef<float, 3> &output, const Vector2 &threshold);
void msdfErrorCorrection_legacy(const BitmapRef<float, 4> &output, const Vector2 &threshold);

/// Performs error correction on a computed MSDF to eliminate interpolation artifacts. This is a low-level class, you may want to use the API in msdf-error-correction.h instead.
class MSDFErrorCorrection {

public:
    /// Stencil flags.
    enum Flags {
        /// Texel marked as potentially causing interpolation errors.
        ERROR = 1,
        /// Texel marked as protected. Protected texels are only given the error flag if they cause inversion artifacts.
        PROTECTED = 2
    };

    MSDFErrorCorrection();
    explicit MSDFErrorCorrection(const BitmapRef<byte, 1> &stencil, const Projection &projection, double range);
    /// Sets the minimum ratio between the actual and maximum expected distance delta to be considered an error.
    void setMinDeviationRatio(double minDeviationRatio);
    /// Sets the minimum ratio between the pre-correction distance error and the post-correction distance error.
    void setMinImproveRatio(double minImproveRatio);
    /// Flags all texels that are interpolated at corners as protected.
    void protectCorners(const Shape &shape);
    /// Flags all texels that contribute to edges as protected.
    template <int N>
    void protectEdges(const BitmapConstRef<float, N> &sdf);
    /// Flags all texels as protected.
    void protectAll();
    /// Flags texels that are expected to cause interpolation artifacts based on analysis of the SDF only.
    template <int N>
    void findErrors(const BitmapConstRef<float, N> &sdf);
    /// Flags texels that are expected to cause interpolation artifacts based on analysis of the SDF and comparison with the exact shape distance.
    template <template <typename> class ContourCombiner, int N>
    void findErrors(const BitmapConstRef<float, N> &sdf, const Shape &shape);
    /// Modifies the MSDF so that all texels with the error flag are converted to single-channel.
    template <int N>
    void apply(const BitmapRef<float, N> &sdf) const;
    /// Returns the stencil in its current state (see Flags).
    BitmapConstRef<byte, 1> getStencil() const;

private:
    BitmapRef<byte, 1> stencil;
    Projection projection;
    double invRange;
    double minDeviationRatio;
    double minImproveRatio;

};

}

#ifdef MSDFGEN_USE_FREETYPE

namespace msdfgen {

typedef unsigned char byte;
typedef unsigned unicode_t;

class FreetypeHandle;
class FontHandle;

class GlyphIndex {

public:
    explicit GlyphIndex(unsigned index = 0);
    unsigned getIndex() const;

private:
    unsigned index;

};

/// Global metrics of a typeface (in font units).
struct FontMetrics {
    /// The size of one EM.
    double emSize;
    /// The vertical position of the ascender and descender relative to the baseline.
    double ascenderY, descenderY;
    /// The vertical difference between consecutive baselines.
    double lineHeight;
    /// The vertical position and thickness of the underline.
    double underlineY, underlineThickness;
};

/// A structure to model a given axis of a variable font.
struct FontVariationAxis {
    /// The name of the variation axis.
    const char *name;
    /// The axis's minimum coordinate value.
    double minValue;
    /// The axis's maximum coordinate value.
    double maxValue;
    /// The axis's default coordinate value. FreeType computes meaningful default values for Adobe MM fonts.
    double defaultValue;
};

/// Initializes the FreeType library.
FreetypeHandle * initializeFreetype();
/// Deinitializes the FreeType library.
void deinitializeFreetype(FreetypeHandle *library);

#ifdef FT_LOAD_DEFAULT // FreeType included
/// Creates a FontHandle from FT_Face that was loaded by the user. destroyFont must still be called but will not affect the FT_Face.
FontHandle * adoptFreetypeFont(FT_Face ftFace);
/// Converts the geometry of FreeType's FT_Outline to a Shape object.
FT_Error readFreetypeOutline(Shape &output, FT_Outline *outline);
#endif

/// Loads a font file and returns its handle.
FontHandle * loadFont(FreetypeHandle *library, const char *filename);
/// Loads a font from binary data and returns its handle.
FontHandle * loadFontData(FreetypeHandle *library, const byte *data, int length);
/// Unloads a font file.
void destroyFont(FontHandle *font);
/// Outputs the metrics of a font file.
bool getFontMetrics(FontMetrics &metrics, FontHandle *font);
/// Outputs the width of the space and tab characters.
bool getFontWhitespaceWidth(double &spaceAdvance, double &tabAdvance, FontHandle *font);
/// Outputs the glyph index corresponding to the specified Unicode character.
bool getGlyphIndex(GlyphIndex &glyphIndex, FontHandle *font, unicode_t unicode);
/// Loads the geometry of a glyph from a font file.
bool loadGlyph(Shape &output, FontHandle *font, GlyphIndex glyphIndex, double *advance = NULL);
bool loadGlyph(Shape &output, FontHandle *font, unicode_t unicode, double *advance = NULL);
/// Outputs the kerning distance adjustment between two specific glyphs.
bool getKerning(double &output, FontHandle *font, GlyphIndex glyphIndex1, GlyphIndex glyphIndex2);
bool getKerning(double &output, FontHandle *font, unicode_t unicode1, unicode_t unicode2);

#ifndef MSDFGEN_DISABLE_VARIABLE_FONTS
/// Sets a single variation axis of a variable font.
bool setFontVariationAxis(FreetypeHandle *library, FontHandle *font, const char *name, double coordinate);
/// Lists names and ranges of variation axes of a variable font.
bool listFontVariationAxes(std::vector<FontVariationAxis> &axes, FreetypeHandle *library, FontHandle *font);
#endif

}

#endif

#ifdef MSDFGEN_USE_SKIA

namespace msdfgen {

/// Resolves any intersections within the shape by subdividing its contours using the Skia library and makes sure its contours have a consistent winding.
bool resolveShapeGeometry(Shape &shape);

}

#endif

namespace msdfgen {

/// Generates a conventional single-channel signed distance field.
void generateSDF(const BitmapRef<float, 1> &output, const Shape &shape, const Projection &projection, double range, const GeneratorConfig &config = GeneratorConfig());

/// Generates a single-channel signed pseudo-distance field.
void generatePseudoSDF(const BitmapRef<float, 1> &output, const Shape &shape, const Projection &projection, double range, const GeneratorConfig &config = GeneratorConfig());

/// Generates a multi-channel signed distance field. Edge colors must be assigned first! (See edgeColoringSimple)
void generateMSDF(const BitmapRef<float, 3> &output, const Shape &shape, const Projection &projection, double range, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());

/// Generates a multi-channel signed distance field with true distance in the alpha channel. Edge colors must be assigned first.
void generateMTSDF(const BitmapRef<float, 4> &output, const Shape &shape, const Projection &projection, double range, const MSDFGeneratorConfig &config = MSDFGeneratorConfig());

// Old version of the function API's kept for backwards compatibility
void generateSDF(const BitmapRef<float, 1> &output, const Shape &shape, double range, const Vector2 &scale, const Vector2 &translate, bool overlapSupport = true);
void generatePseudoSDF(const BitmapRef<float, 1> &output, const Shape &shape, double range, const Vector2 &scale, const Vector2 &translate, bool overlapSupport = true);
void generateMSDF(const BitmapRef<float, 3> &output, const Shape &shape, double range, const Vector2 &scale, const Vector2 &translate, const ErrorCorrectionConfig &errorCorrectionConfig = ErrorCorrectionConfig(), bool overlapSupport = true);
void generateMTSDF(const BitmapRef<float, 4> &output, const Shape &shape, double range, const Vector2 &scale, const Vector2 &translate, const ErrorCorrectionConfig &errorCorrectionConfig = ErrorCorrectionConfig(), bool overlapSupport = true);

// Original simpler versions of the previous functions, which work well under normal circumstances, but cannot deal with overlapping contours.
void generateSDF_legacy(const BitmapRef<float, 1> &output, const Shape &shape, double range, const Vector2 &scale, const Vector2 &translate);
void generatePseudoSDF_legacy(const BitmapRef<float, 1> &output, const Shape &shape, double range, const Vector2 &scale, const Vector2 &translate);
void generateMSDF_legacy(const BitmapRef<float, 3> &output, const Shape &shape, double range, const Vector2 &scale, const Vector2 &translate, ErrorCorrectionConfig errorCorrectionConfig = ErrorCorrectionConfig());
void generateMTSDF_legacy(const BitmapRef<float, 4> &output, const Shape &shape, double range, const Vector2 &scale, const Vector2 &translate, ErrorCorrectionConfig errorCorrectionConfig = ErrorCorrectionConfig());

}
