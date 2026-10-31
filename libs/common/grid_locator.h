#pragma once

#include <QChar>
#include <QString>
#include <QtGlobal>

#include <cmath>

// Maidenhead (grid locator) conversion shared by the launcher and the tracker.
//
// The station position itself is always stored as decimal degrees: locators are
// only an input/display convenience for operators who already know their grid
// square. Encoding uses the standard 20x10 degree field, 2x1 degree square,
// 5x2.5 minute subsquare and 30x15 second extended square. Decoding returns the
// centre of the addressed cell, so round-tripping a position moves it by at
// most half a cell at the requested precision.
namespace asrtu
{
namespace grid
{

constexpr int kLocatorSquareCharacters = 4;
constexpr int kLocatorSubsquareCharacters = 6;
constexpr int kLocatorExtendedCharacters = 8;

inline bool isLocatorLength(int characters)
{
	return characters == kLocatorSquareCharacters ||
	       characters == kLocatorSubsquareCharacters ||
	       characters == kLocatorExtendedCharacters;
}

// Uppercase the locator and drop separators so "ol 63-pd" becomes "OL63PD".
inline QString normalize(const QString &locator)
{
	QString normalized;
	normalized.reserve(8);
	for (const QChar character : locator) {
		if (character.isSpace() || character == QLatin1Char('-') ||
		    character == QLatin1Char('_'))
			continue;
		normalized.append(character.toUpper());
	}
	return normalized;
}

// Encode a position as a 4, 6 or 8 character locator. Returns an empty string
// for an unsupported precision or non-finite coordinates.
inline QString encode(double latitudeDegrees, double longitudeDegrees,
		      int characters = kLocatorSubsquareCharacters)
{
	if (!isLocatorLength(characters) || !std::isfinite(latitudeDegrees) ||
	    !std::isfinite(longitudeDegrees))
		return {};
	// The north pole and the antimeridian address the last cell of the
	// grid.
	double longitude =
	    qBound(-180.0, longitudeDegrees, 180.0 - 1e-9) + 180.0;
	double latitude = qBound(-90.0, latitudeDegrees, 90.0 - 1e-9) + 90.0;

	QString locator;
	locator.reserve(characters);

	int longitudeIndex = qBound(0, int(longitude / 20.0), 17);
	int latitudeIndex = qBound(0, int(latitude / 10.0), 17);
	locator.append(QChar(ushort('A' + longitudeIndex)));
	locator.append(QChar(ushort('A' + latitudeIndex)));
	longitude -= longitudeIndex * 20.0;
	latitude -= latitudeIndex * 10.0;

	if (characters >= kLocatorSquareCharacters) {
		longitudeIndex = qBound(0, int(longitude / 2.0), 9);
		latitudeIndex = qBound(0, int(latitude), 9);
		locator.append(QChar(ushort('0' + longitudeIndex)));
		locator.append(QChar(ushort('0' + latitudeIndex)));
		longitude -= longitudeIndex * 2.0;
		latitude -= latitudeIndex;
	}

	if (characters >= kLocatorSubsquareCharacters) {
		const double longitudeWidth = 2.0 / 24.0;
		const double latitudeHeight = 1.0 / 24.0;
		longitudeIndex = qBound(0, int(longitude / longitudeWidth), 23);
		latitudeIndex = qBound(0, int(latitude / latitudeHeight), 23);
		locator.append(QChar(ushort('A' + longitudeIndex)));
		locator.append(QChar(ushort('A' + latitudeIndex)));
		longitude -= longitudeIndex * longitudeWidth;
		latitude -= latitudeIndex * latitudeHeight;
	}

	if (characters >= kLocatorExtendedCharacters) {
		const double longitudeWidth = 2.0 / 240.0;
		const double latitudeHeight = 1.0 / 240.0;
		longitudeIndex = qBound(0, int(longitude / longitudeWidth), 9);
		latitudeIndex = qBound(0, int(latitude / latitudeHeight), 9);
		locator.append(QChar(ushort('0' + longitudeIndex)));
		locator.append(QChar(ushort('0' + latitudeIndex)));
	}

	return locator;
}

// Decode a locator and write the centre of its cell. Error strings are
// language-neutral: the caller shows its own translated message.
inline bool decode(const QString &locator, double *latitudeDegrees,
		   double *longitudeDegrees, QString *error = nullptr)
{
	const auto fail = [error](const QString &message) {
		if (error)
			*error = message;
		return false;
	};

	const QString normalized = normalize(locator);
	if (normalized.isEmpty())
		return fail(QStringLiteral("empty locator"));
	const int characters = normalized.size();
	if (!isLocatorLength(characters))
		return fail(
		    QStringLiteral("locator must have 4, 6 or 8 characters"));

	const int pairs = characters / 2;
	for (int pair = 0; pair < pairs; ++pair) {
		const QChar first = normalized.at(pair * 2);
		const QChar second = normalized.at(pair * 2 + 1);
		if (pair % 2 == 1) {
			for (const QChar character : {first, second}) {
				if (character.unicode() < '0' ||
				    character.unicode() > '9')
					return fail(QStringLiteral(
					    "locator digit pair is invalid"));
			}
			continue;
		}
		const int limit = pair == 0 ? 17 : 23; // 'R' and 'X'
		for (const QChar character : {first, second}) {
			if (character.unicode() < 'A' ||
			    character.unicode() > 'A' + limit)
				return fail(QStringLiteral(
				    "locator letter pair is invalid"));
		}
	}

	double longitude = -180.0;
	double latitude = -90.0;
	double longitudeWidth = 20.0;
	double latitudeHeight = 10.0;
	for (int pair = 0; pair < pairs; ++pair) {
		const QChar first = normalized.at(pair * 2);
		const QChar second = normalized.at(pair * 2 + 1);
		if (pair % 2 == 0) {
			longitude += (first.unicode() - 'A') * longitudeWidth;
			latitude += (second.unicode() - 'A') * latitudeHeight;
		} else {
			longitude += first.digitValue() * longitudeWidth;
			latitude += second.digitValue() * latitudeHeight;
		}
		if (pair + 1 < pairs) {
			const bool digits = pair % 2 == 1;
			longitudeWidth /= digits ? 24.0 : 10.0;
			latitudeHeight /= digits ? 24.0 : 10.0;
		}
	}

	if (latitudeDegrees)
		*latitudeDegrees = latitude + latitudeHeight / 2.0;
	if (longitudeDegrees)
		*longitudeDegrees = longitude + longitudeWidth / 2.0;
	return true;
}

} // namespace grid
} // namespace asrtu