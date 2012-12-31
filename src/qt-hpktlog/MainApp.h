#ifndef	__MAINAPP_H__
#define	__MAINAPP_H__

#include <vector>
#include <string>

#include <pcap.h>
#include <sys/types.h>

#include <QtCore/QObject>
#include <QtGui/QMainWindow>
#include <QtCore/QVector>

#include "qwt_plot.h"
#include "qwt_plot_curve.h"
#include "qwt_plot_spectrocurve.h"
#include "qwt_plot_histogram.h"
#include "qwt_symbol.h"
#include "qwt_point_3d.h"

#include "libradarpkt/pkt.h"

#include "HeatMap.h"

class MainApp : public QMainWindow
{
	Q_OBJECT

	private:
		// Why can't we just use references, rather than
		// pointers?
		QwtPlot *q_plot;
		QwtPlotSpectroCurve *q_curve;
		QwtSymbol *q_symbol;

		// How many entries to keep in the histogram
		int num_entries;

		// Our old-style histogram data
		std::vector<double> q_dur;
		std::vector<double> q_rssi;

		// and the new-style histogram data
		QVector<QwtPoint3D> q_points;

		// "Heat" map data
		HeatMap *hm;

		// TODO	When rendering the screen, we only want to do it
		//	every say, 3ms.

	public:
		MainApp(QMainWindow *parent = 0);
		~MainApp();

		// Replot the screen.  This does the actual work of
		// taking the current set of dur/rssi values and plotting
		// them.

		// It doesn't do any pacing of the rendering requests.
		void RePlot();

		void timerEvent(QTimerEvent *event);

		void SetTitle(std::string s);

	public slots:
		void getRadarEntry(struct radar_entry re);

};

#endif	/* __MAINAPP_H__ */
