#include <QtGui/QWidget>
#include <QtGui/QFileDialog>
#include <QtGui/QMessageBox>
#include <QtGui/QMainWindow>

#include "qwt_plot.h"
#include "qwt_plot_curve.h"
#include "qwt_plot_spectrocurve.h"
#include "qwt_plot_histogram.h"
#include "qwt_symbol.h"

#include "MainApp.h"

MainApp::MainApp(QMainWindow *parent)
{

	// How many entries to keep in the FIFO
	num_entries = 128;

	// Create window
	q_plot = new QwtPlot(QwtText("example"));
        q_plot->setTitle("Example");

	// Default size
	q_plot->setGeometry(0, 0, 640, 400);

	// Scale
	// y-scale?
	q_plot->setAxisScale(QwtPlot::xBottom, 0.0, 256.0);
	q_plot->setAxisScale(QwtPlot::yLeft, -16.0, 80.0);

	// The default is a single 1 pixel dot.
	// This makes it very difficult to see.
	q_symbol = new QwtSymbol();
	q_symbol->setStyle(QwtSymbol::Cross);
	q_symbol->setSize(2, 2);

	// And now, the default curve
	q_curve = new QwtPlotSpectroCurve("curve");
	//q_curve->setStyle(QwtPlotCurve::Dots);
	//q_curve->setSymbol(q_symbol);
	q_curve->setPenWidth(4);
	q_curve->attach(q_plot);

	q_plot->show();
}

MainApp::~MainApp()
{

	/* XXX correct order? */
	if (q_symbol)
		delete q_symbol;
	if (q_curve)
		delete q_curve;
	if (q_plot)
		delete q_plot;
}

//
// This causes the radar entry to get received and replotted.
// It's quite possible we should just fire off a 1ms timer event
// _after_ this occurs, in case we get squeezed a whole set of
// radar entries. Noone will notice if we only update every 1ms,
// right?
void
MainApp::getRadarEntry(struct radar_entry re)
{

	//printf("%s: called!\n", __func__);

	// Add it to the start duration/rssi array
	q_dur.insert(q_dur.begin(), (float) re.re_dur);
	q_rssi.insert(q_rssi.begin(), (float) re.re_rssi);

	q_points.insert(q_points.begin(),
	    QwtPoint3D(
	    (float) re.re_dur,
	    (float) re.re_rssi,
	    (float) re.re_rssi * 25.0));

	// If we're too big, delete the first entry
	if (q_points.size() > num_entries) {
		q_dur.pop_back();
		q_rssi.pop_back();
		q_points.pop_back();
	}

	// Trim the head entries if the array is too big
	// (maybe we should use a queue, not a vector?)

	// Replot!
	RePlot();
}

void
MainApp::RePlot()
{
	// Plot them
	q_curve->setSamples(q_points);

	/* Plot */
	q_plot->replot();
	q_plot->show();
}

void
MainApp::timerEvent(QTimerEvent *event)
{

}
