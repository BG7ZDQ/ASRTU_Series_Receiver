#include "grid_locator.h"

#include <QString>

#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <cmath>
#include <iostream>

namespace
{

bool near(double actual, double expected, double tolerance = 1e-6)
{
	return std::fabs(actual - expected) <= tolerance;
}

void checkEncode(double latitude, double longitude, int characters,
		 const QString &expected)
{
	const QString encoded =
	    asrtu::grid::encode(latitude, longitude, characters);
	if (encoded != expected) {
		std::cerr << "encode(" << latitude << ", " << longitude << ", "
			  << characters << ") = " << encoded.toStdString()
			  << ", expected " << expected.toStdString() << '\n';
		assert(false);
	}
}

void checkDecode(const QString &locator, double expectedLatitude,
		 double expectedLongitude)
{
	double latitude = 0.0;
	double longitude = 0.0;
	QString error;
	const bool decoded =
	    asrtu::grid::decode(locator, &latitude, &longitude, &error);
	assert(decoded);
	assert(error.isEmpty());
	if (!near(latitude, expectedLatitude) ||
	    !near(longitude, expectedLongitude)) {
		std::cerr << "decode(" << locator.toStdString()
			  << ") = " << latitude << ", " << longitude
			  << ", expected " << expectedLatitude << ", "
			  << expectedLongitude << '\n';
		assert(false);
	}
}

void checkInvalid(const QString &locator)
{
	double latitude = 0.0;
	double longitude = 0.0;
	QString error;
	assert(!asrtu::grid::decode(locator, &latitude, &longitude, &error));
	assert(!error.isEmpty());
}

} // namespace

int main()
{
	// Guangzhou, Beijing and London addresses: the reference values used by
	// the operator documentation.
	checkEncode(23.1291, 113.2644, 4, QStringLiteral("OL63"));
	checkEncode(23.1291, 113.2644, 6, QStringLiteral("OL63PD"));
	checkEncode(39.9042, 116.4074, 6, QStringLiteral("OM89EV"));
	checkEncode(51.5074, -0.1278, 6, QStringLiteral("IO91WM"));

	// Decoding always resolves to the centre of the addressed cell.
	checkDecode(QStringLiteral("AA00"), -89.5, -179.0);
	checkDecode(QStringLiteral("IO91"), 51.5, -1.0);
	checkDecode(QStringLiteral("IO91WM"), 51.520833, -0.125);
	checkDecode(QStringLiteral("OL63"), 23.5, 113.0);
	checkDecode(QStringLiteral("OL63PD"), 23.145833, 113.291667);
	checkDecode(QStringLiteral("OM89"), 39.5, 117.0);
	checkDecode(QStringLiteral("OM89EV"), 39.895833, 116.375);
	checkDecode(QStringLiteral("OL63PD12"), 23.135417, 113.2625);

	// Lowercase and separator tolerant input.
	checkDecode(QStringLiteral(" ol 63-pd "), 23.145833, 113.291667);
	assert(asrtu::grid::normalize(QStringLiteral("ol_63PD")) ==
	       QStringLiteral("OL63PD"));

	// The poles and the antimeridian address the last cells of the grid.
	checkEncode(90.0, 180.0, 6, QStringLiteral("RR99XX"));
	checkEncode(90.0, 180.0, 8, QStringLiteral("RR99XX99"));
	checkEncode(-90.0, -180.0, 8, QStringLiteral("AA00AA00"));

	// Every position must re-encode to the cell its centre belongs to.
	const double positions[][2] = {
	    {23.1291, 113.2644},  {-33.8688, 151.2093}, {64.1466, -21.9426},
	    {-1.2921, 36.8219},	  {0.0, 0.0},		{78.2232, 15.6469},
	    {-54.8019, -68.3030}, {35.6895, 139.6917},
	};
	for (const auto &position : positions) {
		for (const int characters : {4, 6, 8}) {
			const QString cell = asrtu::grid::encode(
			    position[0], position[1], characters);
			assert(cell.size() == characters);
			const double width = characters == 4   ? 2.0
					     : characters == 6 ? 2.0 / 24.0
							       : 2.0 / 240.0;
			double latitude = 0.0;
			double longitude = 0.0;
			assert(
			    asrtu::grid::decode(cell, &latitude, &longitude));
			assert(asrtu::grid::encode(latitude, longitude,
						   characters) == cell);
			assert(std::fabs(latitude - position[0]) <
			       width); // half a cell plus rounding slack
			assert(std::fabs(longitude - position[1]) <
			       width * 2.0); // cells are twice as wide as tall
		}
	}

	// Rejected input: unsupported lengths, wrong letters and stray symbols.
	checkInvalid(QString());
	checkInvalid(QStringLiteral("OL6"));
	checkInvalid(QStringLiteral("OL63P"));
	checkInvalid(QStringLiteral("OL63PD1"));
	checkInvalid(QStringLiteral("SS00"));
	checkInvalid(QStringLiteral("OL63QZ"));
	checkInvalid(QStringLiteral("OL63P1"));
	checkInvalid(QStringLiteral("OL63P!"));
	checkInvalid(QStringLiteral("12OZ"));
	assert(asrtu::grid::encode(0.0, 0.0, 5).isEmpty());
	assert(asrtu::grid::encode(0.0, 0.0, 3).isEmpty());
	assert(asrtu::grid::encode(std::nan(""), 0.0).isEmpty());
	assert(asrtu::grid::encode(0.0, std::nan("")).isEmpty());
	return 0;
}