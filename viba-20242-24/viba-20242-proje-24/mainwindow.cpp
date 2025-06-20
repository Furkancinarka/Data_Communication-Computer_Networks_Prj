#include "mainwindow.h"
#include <QMessageBox>
#include <QProgressBar>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QDateTime>
#include <QFileInfo>
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , centralWidget(new QWidget(this))
    , mainLayout(new QVBoxLayout(centralWidget))
    , buttonLayout(new QHBoxLayout())
    , openFileButton(new QPushButton("Open File", this))
    , processButton(new QPushButton("Process Data", this))
    , simulateButton(new QPushButton("Start Transmission", this))
    , frameList(new QListWidget(this))
    , checksumLabel(new QLabel("Checksum: Not calculated", this))
    , statusLabel(new QLabel("Status: Ready", this))
    , progressBar(new QProgressBar(this))
    , datalinkLayer(new DataLinkLayer(this))
    , detailsTabWidget(new QTabWidget)
    , frameDetailsText(new QTextEdit)
    , statisticsText(new QTextEdit)
    , errorLogText(new QTextEdit)
    , visualizationLayout(new QHBoxLayout())
    , scene(new QGraphicsScene(this))
    , visualizationView(new QGraphicsView(scene))
    , senderLabel(new QLabel("Sender", this))
    , receiverLabel(new QLabel("Receiver", this))
    , sendingScene(new QGraphicsScene(this))
    , receivingScene(new QGraphicsScene(this))
{
    setCentralWidget(centralWidget);
    setupUI();
    createConnections();

    // Initialize visualization scenes
    sendingScene->setSceneRect(0, 0, 400, 200);
    receivingScene->setSceneRect(0, 0, 400, 200);
    
    // Set up visualization view
    visualizationView->setScene(sendingScene);
    visualizationView->setMinimumSize(400, 200);
    visualizationView->setRenderHint(QPainter::Antialiasing);
    visualizationView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    visualizationView->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
}

MainWindow::~MainWindow()
{
}

void MainWindow::setupUI()
{
    // Set window properties
    setWindowTitle("Data Link Layer Simulator");
    resize(1000, 800);

    // Configure progress bar
    progressBar->setVisible(false);
    progressBar->setRange(0, 0); // Indeterminate progress

    // Add buttons to button layout
    buttonLayout->addWidget(openFileButton);
    buttonLayout->addWidget(processButton);
    buttonLayout->addWidget(simulateButton);

    // Add widgets to main layout
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(progressBar);
    mainLayout->addWidget(frameList);
    mainLayout->addWidget(checksumLabel);
    mainLayout->addWidget(statusLabel);

    // Configure frame list
    frameList->setAlternatingRowColors(true);
    frameList->setSelectionMode(QAbstractItemView::SingleSelection);
    frameList->setFont(QFont("Consolas", 10));

    // Set initial button states
    processButton->setEnabled(false);
    simulateButton->setEnabled(false);

    // Add details tab widget
    detailsTabWidget->addTab(frameDetailsText, "Frame Details");
    detailsTabWidget->addTab(statisticsText, "Statistics");
    detailsTabWidget->addTab(errorLogText, "Error Log");

    mainLayout->addWidget(detailsTabWidget);

    // Create visualization layout
    visualizationLayout = new QHBoxLayout();
    
    // Create and setup visualization components
    visualizationView->setMinimumSize(400, 200);
    visualizationView->setRenderHint(QPainter::Antialiasing);
    
    QVBoxLayout *labelsLayout = new QVBoxLayout();
    labelsLayout->addWidget(senderLabel);
    labelsLayout->addWidget(visualizationView);
    labelsLayout->addWidget(receiverLabel);
    
    visualizationLayout->addLayout(labelsLayout);
    mainLayout->addLayout(visualizationLayout);

    // Initialize DataLinkLayer
}

void MainWindow::createConnections()
{
    connect(openFileButton, &QPushButton::clicked, this, &MainWindow::openFile);
    connect(processButton, &QPushButton::clicked, this, &MainWindow::processData);
    connect(simulateButton, &QPushButton::clicked, this, &MainWindow::simulateTransmission);
    connect(frameList, &QListWidget::itemClicked, this, &MainWindow::onFrameSelected);

    // Connect MainWindow signals
    connect(this, &MainWindow::errorOccurred, this, &MainWindow::onErrorOccurred);

    // Connect DataLinkLayer signals
    connect(datalinkLayer, &DataLinkLayer::frameProcessed, this, &MainWindow::updateFrameStatus);
    connect(datalinkLayer, &DataLinkLayer::transmissionComplete, this, &MainWindow::onTransmissionComplete);
    connect(datalinkLayer, &DataLinkLayer::errorOccurred, this, &MainWindow::onErrorOccurred);
    connect(datalinkLayer, &DataLinkLayer::statusUpdate, this, &MainWindow::onStatusUpdate);
    connect(datalinkLayer, &DataLinkLayer::checksumCalculated, this, &MainWindow::onChecksumCalculated);
    connect(datalinkLayer, &DataLinkLayer::checksumFrameSent, this, &MainWindow::onChecksumFrameSent);
}

void MainWindow::openFile()
{
    QString filePath = QFileDialog::getOpenFileName(this, "Open File", QString(), "All Files (*.*)");
    if (!filePath.isEmpty()) {
        currentFilePath = filePath;
        processButton->setEnabled(true);
        statusLabel->setText("Status: File selected - " + QFileInfo(filePath).fileName());
    }
}

void MainWindow::processData()
{
    if (currentFilePath.isEmpty()) {
        QMessageBox::warning(this, "Error", "No file selected");
        return;
    }

    if (!datalinkLayer->loadFile(currentFilePath)) {
        QMessageBox::warning(this, "Error", "Failed to load file");
        return;
    }

    auto frames = datalinkLayer->getFrames();
    if (frames.isEmpty()) {
        QMessageBox::warning(this, "Error", "No frames to process");
        return;
    }

    totalFrames = frames.size();
    processedFrames = 0;
    stats = Statistics(); // Reset statistics
    stats.totalFrames = totalFrames; // Set total frames for statistics
    
    qDebug() << "ProcessData - Total Frames:" << totalFrames;
    qDebug() << "ProcessData - Stats Total Frames:" << stats.totalFrames;
    
    frameList->clear();
    progressBar->setRange(0, 100); // Set range to percentage
    progressBar->setValue(0);
    progressBar->setVisible(true); // Show progress bar
    
    simulateButton->setEnabled(true);
    statusLabel->setText(QString("Status: Ready to transmit - %1 frames loaded").arg(totalFrames));
    
    // Update statistics immediately after setting totalFrames
    updateStatistics();
}

void MainWindow::simulateTransmission()
{
    try {
        if (!datalinkLayer) {
            qDebug() << "Error: DataLinkLayer is not initialized";
            emit errorOccurred("DataLinkLayer is not initialized");
            return;
        }

        if (!datalinkLayer->isTransmitting()) {
            qDebug() << "Starting transmission...";
            
            if (totalFrames == 0) {
                qDebug() << "Error: No frames to transmit";
                emit errorOccurred("No frames to transmit");
                return;
            }

            clearVisualization();
            frameList->clear();
            processedFrames = 0;
            progressBar->setValue(0);
            progressBar->setVisible(true); // Show progress bar
            stats = Statistics(); // Reset statistics
            stats.totalFrames = totalFrames; // Set total frames for statistics
            qDebug() << "SimulateTransmission - Total Frames:" << totalFrames;
            qDebug() << "SimulateTransmission - Stats Total Frames:" << stats.totalFrames;
            
            try {
                datalinkLayer->startTransmission();
                simulateButton->setText("Stop Transmission");
                statusLabel->setText("Status: Transmission in progress...");
                qDebug() << "Transmission started successfully";
            } catch (const std::exception& e) {
                qDebug() << "Error starting transmission:" << e.what();
                emit errorOccurred(QString("Transmission failed: %1").arg(e.what()));
                return;
            }
        } else {
            qDebug() << "Stopping transmission...";
            try {
                datalinkLayer->stopTransmission();
                simulateButton->setText("Start Transmission");
                statusLabel->setText("Status: Transmission stopped");
                progressBar->setVisible(false); // Hide progress bar
                qDebug() << "Transmission stopped successfully";
            } catch (const std::exception& e) {
                qDebug() << "Error stopping transmission:" << e.what();
                emit errorOccurred(QString("Error stopping transmission: %1").arg(e.what()));
            }
        }
    } catch (const std::exception& e) {
        qDebug() << "Critical error in simulateTransmission:" << e.what();
        emit errorOccurred(QString("Critical error: %1").arg(e.what()));
    }
}

void MainWindow::onFrameSelected(QListWidgetItem *item)
{
    try {
        // Parse frame number from the item text
        QStringList parts = item->text().split(" ");
        int frameNumber = parts[1].replace(":", "").toInt();
        
        qDebug() << "Selected frame number:" << frameNumber;
        
        auto frames = datalinkLayer->getFrames();
        if (frameNumber >= 0 && frameNumber < frames.size()) {
            showFrameDetails(frames[frameNumber]);
        } else {
            qDebug() << "Invalid frame number:" << frameNumber;
            emit errorOccurred(QString("Invalid frame number: %1").arg(frameNumber));
        }
    } catch (const std::exception& e) {
        qDebug() << "Error in onFrameSelected:" << e.what();
        emit errorOccurred(QString("Error selecting frame: %1").arg(e.what()));
    }
}

void MainWindow::showFrameDetails(const Frame &frame)
{
    try {
        QString details;
        details += QString("=== Frame %1 Details ===\n\n").arg(frame.getFrameNumber());
        
        // Basic Information
        details += QString("Frame Number: %1\n").arg(frame.getFrameNumber());
        details += QString("Bit Count: %1\n").arg(frame.getBitCount());
        details += QString("CRC: %1\n").arg(frame.getCRC());
        details += QString("Status: %1\n").arg(frame.isValid() ? "Valid" : "Invalid");
        details += QString("Type: %1\n").arg(frame.isLastFrame() ? "Partial Frame" : "Full Frame");
        
        // Data Information
        details += "\nData:\n";
        details += QString("Hex: %1\n").arg(frame.getHexData());
        details += QString("Binary: %1\n").arg(frame.getBinaryData());
        
        // Error Information
        if (!frame.getErrorInfo().isEmpty()) {
            details += "\nError Information:\n";
            for (const auto &error : frame.getErrorInfo().toStdMap()) {
                details += QString("%1: %2\n").arg(error.first).arg(error.second);
            }
        }
        
        frameDetailsText->setText(details);
        qDebug() << "Displayed details for frame" << frame.getFrameNumber();
    } catch (const std::exception& e) {
        qDebug() << "Error showing frame details:" << e.what();
        emit errorOccurred(QString("Error showing frame details: %1").arg(e.what()));
    }
}

void MainWindow::updateStatistics()
{
    try {
        QString statsText;
        statsText += "=== Transmission Statistics ===\n\n";
        
        // Ensure totalFrames is not zero to avoid division by zero
        if (stats.totalFrames > 0) {
            statsText += QString("Total Frames: %1\n").arg(stats.totalFrames);
            statsText += QString("Successful Frames: %1 (%2%)\n")
                .arg(stats.successfulFrames)
                .arg((stats.successfulFrames * 100.0) / stats.totalFrames, 0, 'f', 2);
            statsText += QString("Lost Frames: %1 (%2%)\n")
                .arg(stats.lostFrames)
                .arg((stats.lostFrames * 100.0) / stats.totalFrames, 0, 'f', 2);
            statsText += QString("Corrupted Frames: %1 (%2%)\n")
                .arg(stats.corruptedFrames)
                .arg((stats.corruptedFrames * 100.0) / stats.totalFrames, 0, 'f', 2);
            statsText += QString("ACK Lost: %1 (%2%)\n")
                .arg(stats.ackLostFrames)
                .arg((stats.ackLostFrames * 100.0) / stats.totalFrames, 0, 'f', 2);
        } else {
            statsText += "Total Frames: 0\n";
            statsText += "Successful Frames: 0 (0%)\n";
            statsText += "Lost Frames: 0 (0%)\n";
            statsText += "Corrupted Frames: 0 (0%)\n";
            statsText += "ACK Lost: 0 (0%)\n";
        }
        
        statsText += QString("Checksum Errors: %1\n").arg(stats.checksumErrors);
        
        statisticsText->setText(statsText);
        qDebug() << "Statistics updated - Total Frames:" << stats.totalFrames;
    } catch (const std::exception& e) {
        qDebug() << "Error updating statistics:" << e.what();
        emit errorOccurred(QString("Error updating statistics: %1").arg(e.what()));
    }
}

void MainWindow::updateFrameStatus(const Frame &frame)
{
    try {
        qDebug() << "Updating status for frame" << frame.getFrameNumber();
        qDebug() << "Current Stats - Total Frames:" << stats.totalFrames;
        
        processedFrames++;
        int progress = (processedFrames * 100) / totalFrames;
        progressBar->setValue(progress);
        
        // Update statistics
        if (frame.isValid()) {
            stats.successfulFrames++;
            qDebug() << "Frame" << frame.getFrameNumber() << "processed successfully";
        } else {
            auto errorInfo = frame.getErrorInfo();
            bool errorCounted = false;
            
            // Check errors in order of priority
            if (errorInfo.contains("Lost") && !errorCounted) {
                stats.lostFrames++;
                errorCounted = true;
                qDebug() << "Frame" << frame.getFrameNumber() << "was lost";
            }
            if (errorInfo.contains("Corrupted") && !errorCounted) {
                stats.corruptedFrames++;
                errorCounted = true;
                qDebug() << "Frame" << frame.getFrameNumber() << "was corrupted";
            }
            if (errorInfo.contains("ACK Lost") && !errorCounted) {
                stats.ackLostFrames++;
                errorCounted = true;
                qDebug() << "Frame" << frame.getFrameNumber() << "ACK was lost";
            }
        }
        
        qDebug() << "Updated Stats - Total Frames:" << stats.totalFrames;
        qDebug() << "Updated Stats - Successful Frames:" << stats.successfulFrames;
        qDebug() << "Updated Stats - Lost Frames:" << stats.lostFrames;
        qDebug() << "Updated Stats - Corrupted Frames:" << stats.corruptedFrames;
        qDebug() << "Updated Stats - ACK Lost Frames:" << stats.ackLostFrames;
        
        updateStatistics();
        
        // Update frame in list
        bool frameFound = false;
        for (int i = 0; i < frameList->count(); ++i) {
            QListWidgetItem *item = frameList->item(i);
            if (item && item->text().startsWith(QString("Frame %1:").arg(frame.getFrameNumber()))) {
                item->setText(frame.toString());
                item->setForeground(frame.isValid() ? Qt::black : Qt::red);
                frameFound = true;
                qDebug() << "Updated existing frame" << frame.getFrameNumber() << "in list";
                break;
            }
        }
        
        if (!frameFound) {
            QListWidgetItem *newItem = new QListWidgetItem(frame.toString());
            newItem->setForeground(frame.isValid() ? Qt::black : Qt::red);
            frameList->addItem(newItem);
            qDebug() << "Added new frame" << frame.getFrameNumber() << "to list";
        }
        
        try {
            updateVisualization(frame, true);
        } catch (const std::exception& e) {
            qDebug() << "Error updating visualization for frame" << frame.getFrameNumber() << ":" << e.what();
            emit errorOccurred(QString("Error updating visualization: %1").arg(e.what()));
        }
        
    } catch (const std::exception& e) {
        qDebug() << "Error in updateFrameStatus:" << e.what();
        emit errorOccurred(QString("Error updating frame status: %1").arg(e.what()));
    }
}

void MainWindow::onErrorOccurred(const QString &error)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    errorLogText->append(QString("[%1] %2").arg(timestamp).arg(error));
    
    if (error.contains("Checksum")) {
        stats.checksumErrors++;
        updateStatistics();
    }
    
    QMessageBox::warning(this, "Error", error);
}

void MainWindow::onTransmissionComplete()
{
    statusLabel->setText("Transmission complete");
    simulateButton->setEnabled(true);
    simulateButton->setText("Start Transmission");
    progressBar->setVisible(false); // Hide progress bar
    updateStatistics();
}

void MainWindow::onStatusUpdate(const QString &status)
{
    statusLabel->setText(status);
    errorLogText->append(QString("[%1] Status: %2")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
        .arg(status));
}

void MainWindow::onChecksumCalculated(const QString &checksum)
{
    checksumLabel->setText(QString("Checksum: 0x%1").arg(checksum.toUpper()));
    errorLogText->append(QString("[%1] Checksum calculated: 0x%2")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
        .arg(checksum.toUpper()));
}

void MainWindow::onChecksumFrameSent(const QString &checksumFrame)
{
    errorLogText->append(QString("[%1] Checksum frame sent: 0x%2")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss.zzz"))
        .arg(checksumFrame.toUpper()));
}

void MainWindow::updateVisualization(const Frame &frame, bool isSending)
{
    try {
        QGraphicsScene *scene = isSending ? sendingScene : receivingScene;
        if (!scene) {
            qDebug() << "Error: Scene is null";
            return;
        }

        // Calculate position with scrolling support
        int yPos = static_cast<int>(frame.getFrameNumber() * 40) % static_cast<int>(scene->height());
        
        // Create frame rectangle
        QGraphicsRectItem *frameRect = new QGraphicsRectItem(0, 0, 40, 30);
        frameRect->setPos(isSending ? 0 : scene->width() - 50, yPos);
        
        // Set color based on frame status
        if (!frame.isValid()) {
            frameRect->setBrush(QBrush(Qt::red));
        } else if (frame.isLastFrame()) {
            frameRect->setBrush(QBrush(Qt::yellow)); // Highlight partial frames
        } else {
            frameRect->setBrush(QBrush(Qt::green));
        }
        
        // Add frame number text
        QGraphicsTextItem *text = new QGraphicsTextItem(QString::number(frame.getFrameNumber()));
        text->setPos(frameRect->pos().x() + 5, frameRect->pos().y() + 5);
        
        // Add to scene
        scene->addItem(frameRect);
        scene->addItem(text);
        
        // Auto-scroll if needed
        if (yPos + 40 > scene->height()) {
            scene->setSceneRect(0, 0, scene->width(), yPos + 50);
        }

        // Switch visualization view to the correct scene
        if (isSending) {
            visualizationView->setScene(sendingScene);
        } else {
            visualizationView->setScene(receivingScene);
        }
        
        // Force update
        scene->update();
        visualizationView->viewport()->update();
        
    } catch (const std::exception& e) {
        qDebug() << "Error in updateVisualization:" << e.what();
        emit errorOccurred(QString("Error updating visualization: %1").arg(e.what()));
    }
}

void MainWindow::clearVisualization()
{
    try {
        if (sendingScene) {
            sendingScene->clear();
        }
        if (receivingScene) {
            receivingScene->clear();
        }
        visualizationView->viewport()->update();
    } catch (const std::exception& e) {
        qDebug() << "Error clearing visualization:" << e.what();
        emit errorOccurred(QString("Error clearing visualization: %1").arg(e.what()));
    }
} 