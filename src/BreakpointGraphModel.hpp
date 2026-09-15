/*
Copyright (C) 2026 Malte Steiner

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#pragma once

#include "HydraDocument.hpp"

#include <QtCore/QAbstractListModel>
#include <QtCore/QPointer>
#include <QtCore/QVariantList>
#include <QtGui/QVector3D>

namespace hydra2 {

class TrackPointModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        XPosRole = Qt::UserRole + 1,
        YPosRole,
        ZPosRole,
        PointIdRole
    };

    TrackPointModel(HydraDocument* document,
                    int partial,
                    TrackKind kind,
                    QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setKind(TrackKind kind);
    void refresh();
    quint64 pointIdAt(int row) const;

private:
    HydraDocument* document_ = nullptr;
    int partial_ = -1;
    TrackKind kind_ = TrackKind::Amplitude;
    int rowCountCache_ = 0;
};

class SelectedPointModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        XPosRole = Qt::UserRole + 1,
        YPosRole,
        ZPosRole
    };

    explicit SelectedPointModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    void rebuild(HydraDocument* document, TrackKind kind);
    PointRef refAt(int row) const;

private:
    QVector<QVector3D> points_;
    QVector<PointRef> refs_;
};

class BreakpointGraphModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(HydraDocument* document READ document WRITE setDocument NOTIFY documentChanged)
    Q_PROPERTY(int mode READ mode WRITE setMode NOTIFY modeChanged)
    Q_PROPERTY(double durationMs READ durationMs NOTIFY graphChanged)
    Q_PROPERTY(double maximumY READ maximumY NOTIFY graphChanged)
    Q_PROPERTY(double maximumZ READ maximumZ NOTIFY graphChanged)
    Q_PROPERTY(QObject* selectedPointModel READ selectedPointModel CONSTANT)
    Q_PROPERTY(QString activeLabel READ activeLabel NOTIFY activeLabelChanged)

public:
    enum Role {
        PartialIndexRole = Qt::UserRole + 1,
        PointModelRole,
        PartialSelectedRole
    };

    explicit BreakpointGraphModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    HydraDocument* document() const { return document_; }
    void setDocument(HydraDocument* document);

    int mode() const { return static_cast<int>(mode_); }
    void setMode(int mode);

    double durationMs() const { return durationMs_; }
    double maximumY() const { return maximumY_; }
    double maximumZ() const { return maximumZ_; }
    QObject* selectedPointModel() { return &selectedPoints_; }
    QString activeLabel() const { return activeLabel_; }

    // Called from the Qt Graphs interaction overlay. The point index is the
    // index in the selected Spline3DSeries, not a breakpoint ID.
    Q_INVOKABLE bool beginPointDrag(int partial,
                                    int pointIndex,
                                    int modifiers,
                                    bool logicSnap);
    Q_INVOKABLE bool beginSelectedPointDrag(int selectedIndex,
                                            int modifiers,
                                            bool logicSnap);
    Q_INVOKABLE bool beginPartialDrag(int partial,
                                      int modifiers,
                                      bool logicSnap);
    Q_INVOKABLE void dragByPixels(double deltaX,
                                  double deltaY,
                                  double viewWidth,
                                  double viewHeight,
                                  double cameraZoomLevel,
                                  bool logicSnap);
    Q_INVOKABLE void endDrag();
    Q_INVOKABLE void clearSelection();

signals:
    void documentChanged();
    void modeChanged();
    void graphChanged();
    void activeLabelChanged();

private:
    void rebuildModels();
    void refreshPartials(const QVector<int>& partials);
    void refreshSelection();
    void updateActiveLabel();
    void recalculateRanges();
    bool additiveFromModifiers(Qt::KeyboardModifiers modifiers) const;
    bool toggleFromModifiers(Qt::KeyboardModifiers modifiers) const;

    QPointer<HydraDocument> document_;
    TrackKind mode_ = TrackKind::Amplitude;
    QVector<TrackPointModel*> pointModels_;
    SelectedPointModel selectedPoints_;
    PointRef activePoint_;
    bool dragging_ = false;
    double durationMs_ = 1.0;
    double maximumY_ = 32767.0;
    double maximumZ_ = 1.0;
    QString activeLabel_;
};

} // namespace hydra2
