#pragma once

class QApplication;
class QTranslator;

bool installSystemTranslation(QApplication& application,
                              QTranslator& translator);
