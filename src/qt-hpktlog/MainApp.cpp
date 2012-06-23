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

	// Blank the heat map
	for (int i = 0; i < MAX_RSSI; i++) {
		for (int j = 0; j < MAX_PULSEDUR; j++) {
			heat_map[i][j] = 0;
		}
	}

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


	// Update the heat map for the current pixel, topping out at 65535
	// entries (ie, don't overflow.)
	if (heat_map[re.re_rssi % MAX_RSSI][re.re_dur % MAX_PULSEDUR] < MAX_HEATCNT)
		heat_map[re.re_rssi % MAX_RSSI][re.re_dur % MAX_PULSEDUR]++;

	q_points.insert(q_points.begin(),
	    QwtPoint3D(
	    (float) re.re_dur,
	    (float) re.re_rssi,
	    (float) heat_map[re.re_rssi % MAX_RSSI][re.re_dur % MAX_PULSEDUR] * 100.0));

	// If we're too big, delete the first entry
	if (q_points.size() > num_entries) {
		// Decrement the heat map entry!
		uint8_t rssi, dur;
		rssi = q_rssi[q_rssi.size() - 1];
		dur = q_dur[q_dur.size() - 1];
		if (heat_map[rssi % MAX_RSSI][dur % MAX_PULSEDUR] > 0)
			heat_map[rssi % MAX_RSSI][dur % MAX_PULSEDUR]--;

		// Remove the tail entry
		q_dur.pop_back();
		q_rssi.pop_back();
		q_points.pop_back();
	}

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
