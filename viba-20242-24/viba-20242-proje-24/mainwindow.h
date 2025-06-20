#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QFileDialog>
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QTimer>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QTextEdit>
#include <QTabWidget>
#include "datalinklayer.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

signals:
    void errorOccurred(const QString &error);

private slots:
    void openFile();
    void processData();
    void simulateTransmission();
    void updateFrameStatus(const Frame &frame);
    void onChecksumCalculated(const QString &checksum);
    void onChecksumFrameSent(const QString &checksumFrame);
    void onTransmissionComplete();
    void onErrorOccurred(const QString &error);
    void onStatusUpdate(const QString &status);
    void onFrameSelected(QListWidgetItem *item);

private:
    void setupUI();
    void createConnections();
    void updateVisualization(const Frame &frame, bool isSending);
    void clearVisualization();
    void updateStatistics();
    void showFrameDetails(const Frame &frame);

    // UI Components
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QHBoxLayout *buttonLayout;
    QHBoxLayout *visualizationLayout;
    QPushButton *openFileButton;
    QPushButton *processButton;
    QPushButton *simulateButton;
    QListWidget *frameList;
    QLabel *checksumLabel;
    QLabel *statusLabel;
    QProgressBar *progressBar;
    
    // Visualization Components
    QGraphicsScene *scene;
    QGraphicsView *visualizationView;
    QLabel *senderLabel;
    QLabel *receiverLabel;

    // New Components for Detailed View
    QTabWidget *detailsTabWidget;
    QTextEdit *frameDetailsText;
    QTextEdit *statisticsText;
    QTextEdit *errorLogText;

    // Data Link Layer
    DataLinkLayer *datalinkLayer;
    int totalFrames;
    int processedFrames;
    QString currentFilePath;
    
    // Statistics
    struct Statistics {
        int totalFrames = 0;
        int successfulFrames = 0;
        int lostFrames = 0;
        int corruptedFrames = 0;
        int ackLostFrames = 0;
        int checksumErrors = 0;
    } stats;

    QGraphicsScene *sendingScene;
    QGraphicsScene *receivingScene;
};

#endif // MAINWINDOW_H 