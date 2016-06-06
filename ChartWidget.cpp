#include "chartwidget.h"

#include <QString>
#include <QTextStream>
#include <QFile>
#include <QMessageBox>
#include <QTimer>
#include <QDebug>


ChartWidget::ChartWidget( QWidget *parent )  // def NULL
	: QOpenGLWidget( parent ),
	  m_numDataPoints( 0 ),
	  m_dimensions( 2 ),
	  m_xDomain( 2.0 ),
	  m_yDomain( 1.0 ),
	  m_yCoverage( 0.8 ),
	  m_tickSize( 1000 ),
	  m_numTicks( 0 ),
	  m_xTickStep( 0.0 ),
	  m_xStep( 0.0 ),
	  m_aspectRatioWidth( 1.0f ),
	  m_aspectRatioHeight( 1.0f ),
	  m_zoomFactor( 1.0f ),
	  m_xPan( 0.0f ),
	  m_yPan( 0.0f ),
	  m_near( 0.1f ),
	  m_far( 1.0f ),
	  m_smoothOn( false ),
	  m_recordingPeak( false ),
	  m_recordingValley( false ),
	  m_currentPeak( 0.0f ),
	  m_currentValley( 0.0f ),
	  m_peakTime( 0 ),
	  m_peakX( 0.0f ),
	  m_peakY( 0.0f ),
	  m_lastPeakX( 0.0f ),
	  m_lastPeakY( 0.0f ),
	  m_valleyX( 0.0f ),
	  m_valleyY( 0.0f ),
	  m_lastValleyX( 0.0f ),
	  m_lastValleyY( 0.0f ),
	  m_timeAtMouse( 0 )
{
	/*
	setSizePolicy( QSizePolicy::MinimumExpanding,
				   QSizePolicy::MinimumExpanding );
				   */
	setFocusPolicy( Qt::StrongFocus );

	QTimer *timer = new QTimer(this);
	connect( timer, SIGNAL( timeout() ),
			 this, SLOT( update() ) );
	timer->start( 1000 );

	// initialize the reverse transform to identity
	m_screenToModel[ 0 ] = 1.0f;
	m_screenToModel[ 1 ] = m_screenToModel[ 2 ] = m_screenToModel[ 3 ] = m_screenToModel[ 4 ] = 0.0f;
	m_screenToModel[ 5 ] = 1.0f;
	m_screenToModel[ 6 ] = m_screenToModel[ 7 ] = m_screenToModel[ 8 ] = m_screenToModel[ 9 ] = 0.0f;
	m_screenToModel[ 10 ] = 1.0f;
	m_screenToModel[ 11 ] = m_screenToModel[ 12 ] = m_screenToModel[ 13 ] = m_screenToModel[ 14 ] = 0.0f;
	m_screenToModel[ 15 ] = 1.0f;

	setMouseTracking( true );
}

ChartWidget::~ChartWidget()
{

}

void ChartWidget::qtslotFileChanged( QString &filename )
{
	if ( filename == QString( "" ) )
		return;

	Q_ASSERT( addSignalFile( filename ) );
}


// displays an error and returns false if the file is formatted incorrectly or could not be opened
bool ChartWidget::readDataFile( const QString &filename,
								QVector<float> &data,
								float &smallestY,
								float &largestY )
{
	QFile file( filename );
	if ( !file.open( QIODevice::ReadOnly ) )
	{
		QString sError( "Could not open file: " );
		sError.append( filename );
		QMessageBox::information( 0, sError, file.errorString() );
		return false;
	}

	QTextStream in( &file );

	smallestY = 1.;
	largestY = -1.;
	bool bOk = true;
	while ( !in.atEnd() )
	{
		QString line = in.readLine();
		QStringList fields = line.split(" ");
		if ( fields.size() != 1 )
		{
			file.close();

			QString sError( "More than one value per line. Ignored: " );
			sError.append( filename );
			QMessageBox::information( 0, sError, file.errorString() );
			return false;
		}

		// float fData = std::atof( fields.at( 0 ).toLocal8Bit().constData() );
		float fData = fields.at( 0 ).toFloat( &bOk );
		if ( !bOk )
		{
			file.close();

			QString sError( "Contains non-numbers. Ignored: " );
			sError.append( filename );
			QMessageBox::information( 0, sError, file.errorString() );
			return false;
		}

		if ( fData > largestY )
			largestY = fData;
		if ( fData < smallestY )
			smallestY = fData;

		data.push_back( fData );
	}  // end while not at EOF

	file.close();

	return true;
}  // end readDataFile


// loads the data of the given file unless the call readDataFile() returns an error, or the
// number of data points is inconsistent w/ that of previously loaded files
// should always return true
bool ChartWidget::addSignalFile( QString &filename )
{
	// only allow loading 7 signal files -- this is an artificial limit based on the number of
	// colors I defined for the signals -- otherwise, there is no limit
	if ( m_vectorSignals.count() + 1 > 7 )
	{
		QString sError( "Maximum number of signal files already loaded. Ignored: " );
		sError.append( filename );
		QFile file( filename );
		QMessageBox::information( 0, sError, file.errorString() );
	}

	// read the data points in this file
	QVector<float> data;
	float smallestY = 0.,
			largestY = 0.;
	bool bOk = readDataFile( filename, data, smallestY, largestY );
	if ( !bOk )
		// ignore bad data file and keep going
		// error has already been displayed by readDataFile()
		return true;

	// make sure the number of data points in all files is consistent
	int dataPoints = data.size();
	if ( m_numDataPoints &&
		 dataPoints != m_numDataPoints )
	{
		QString sError( "Number of data points is not the same as that of previously loaded files. Ignored: " );
		sError.append( filename );
		QFile file( filename );
		QMessageBox::information( 0, sError, file.errorString() );
		return true;
	}

	if ( !m_numDataPoints )
	{
		// this is the 1st file read so set the chart parameters
		m_numDataPoints = dataPoints;

		// the X axis tick size is the number of data points between
		// tick marks
		QString sDataPoints =
			QString::fromStdString( std::to_string( m_numDataPoints ) );
		int length = sDataPoints.size();
		m_tickSize = (int) floor( pow( 10, length-1 ) );
		Q_ASSERT( m_tickSize > 0 );

		// set the number of ticks for the graph
		m_numTicks = m_numDataPoints / m_tickSize;
		if ( m_numDataPoints % m_tickSize )
			m_numTicks++;
		Q_ASSERT( m_numTicks > 0 );

		// set the distance between tick marks on the X axis
		m_xTickStep = m_xDomain / m_numTicks;

		// set the distance between data points on the X axis
		m_xStep = m_xTickStep / m_tickSize;
		Q_ASSERT( m_xStep > 0.0f );
	}

	// add the data points to the data point structure
	QVector<float> vectorEmpty;
	m_vectorSignals.push_back( vectorEmpty );
	m_vectorSignals.last().swap( data );

	// add the signal scale factor for this data file
	float signalScale = m_yDomain * .5 * m_yCoverage;
	if ( fabs( smallestY ) > largestY )
		signalScale /= fabs( smallestY );
	else
		signalScale /= largestY;
	m_vectorScales.push_back( signalScale );

	// refresh the screen w/ the new data
	update();

	return true;
}  // end addSignalFile

QSize ChartWidget::minimumSizeHint() const
{
	return QSize(50, 50);
}

QSize ChartWidget::sizeHint() const
{
	return QSize(400, 400);
}


/* -- code for managing the display starts here ----------------------------------*/

void ChartWidget::initializeGL()
{
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	/* tried to use these to get the zoomed in viewport to work
	glEnable( GL_DEPTH_TEST );
	glDepthMask( GL_TRUE );
	glDepthFunc( GL_LESS );
	glDepthRange( 0.0f, 0.1f );
	*/
}

void ChartWidget::resizeGL( int width, int height )
{
	glViewport( 0, 0, width, height );
	setProjectionMatrix( width, height, m_zoomFactor );

	// update the inverse transform
	updateInverseTransform();
}

void ChartWidget::setProjectionMatrix( int width, int height, float zoomFactor )
{
	glMatrixMode( GL_PROJECTION );
	glLoadIdentity();

	// set the aspect ratio corrected width and height
	updateAspectRatioWidthHeight( width, height );

	glOrtho( -m_aspectRatioWidth * zoomFactor, m_aspectRatioWidth * zoomFactor,
			 -m_aspectRatioHeight * zoomFactor, m_aspectRatioHeight * zoomFactor,
			 m_near, m_far );
}

void ChartWidget::updateAspectRatioWidthHeight( int width, int height )
{
	if ( height < 1 )
		return;

	if ( width > height )
		m_aspectRatioWidth = ((float) width ) / height;
	else
		m_aspectRatioHeight = ((float) height ) / width;
}

void ChartWidget::updateInverseTransform()
{
	// get the inverse of the glOrtho projection matrix
	float inverseProjection[ 16 ];
	getInverseProjectionMatrix( inverseProjection );

	// concatenate the inverse glOrtho matrix w/ the inverse model view matrix
/*
	float inverseModelView[] =
	{ 1.0f, 0.0f, 0.0f, 1.0f - m_xPan,
	  0.0f, 1.0f, 0.0f, -m_yPan,
	  0.0f, 0.0f, 1.0f, 1.0f,
	  0.0f, 0.0f, 0.0f, 1.0f };
*/

	m_screenToModel[ 0 ] = inverseProjection[0];
	m_screenToModel[ 3 ] = 1.0f - m_xPan;
	m_screenToModel[ 5 ] = inverseProjection[5];
	m_screenToModel[ 7 ] = -m_yPan;
	m_screenToModel[ 10 ] = inverseProjection[10];
	m_screenToModel[ 11 ] = inverseProjection[10] + inverseProjection[11];
/*
	m_screenToModel =
	{ inverseProjection[0], 0., 0., 1.0f - m_xPan,
	  0., inverseProjection[5], 0., -m_yPan,
	  0., 0., inverseProjection[10], inverseProjection[10] + inverseProjection[11],
	  0., 0., 0., 1. };
*/
}  // end updateInverseTransform

void ChartWidget::setModelViewMatrix()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	// glClear( GL_COLOR_BUFFER_BIT );

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef( -1.0f + m_xPan, m_yPan, -1.0 );

	// update the inverse transform
	updateInverseTransform();
}

// returns the 4x4 inverse projection matrix
void ChartWidget::getInverseProjectionMatrix( float inverseProject[] )
{
	/* inverse perpective transform code
	 *
	Based off http://bookofhook.com/mousepick.pdf
	OpenGL matrix order
	m0 m4 m8 m12
	m1 m5 m9 m13
	m2 m6 m10 m14
	m3 m7 m11 m15

	Projection matrix
	a 0 0 0
	0 b 0 0
	0 0 c d
	0 0 e 0

	Inverse projection
	1/a, 0, 0, 0,
	0, 1/b, 0, 0,
	0, 0, 0, 1/e,
	0, 0, 1/d, -c/(d*e)
	*/

	/*
	double projectionMatrix[16]; // original projection matrix
	glGetDoublev( GL_PROJECTION_MATRIX, projectionMatrix );

	double a = projectionMatrix[ 0 ];
	double b = projectionMatrix[ 5 ];
	double c = projectionMatrix[ 10 ];
	double d = projectionMatrix[ 14 ];
	double e = projectionMatrix[ 11 ];

	// double inverseProject[16] = { 0.0 };
	inverseProject[ 0 ] = 1.0 / a;
	inverseProject[ 1 ] = 0.;
	inverseProject[ 2 ] = 0.;
	inverseProject[ 3 ] = 0.;
	inverseProject[ 4 ] = 0.;
	inverseProject[ 5 ] = 1.0 / b;
	inverseProject[ 6 ] = 0.;
	inverseProject[ 7 ] = 0.;
	inverseProject[ 8 ] = 0.;
	inverseProject[ 9 ] = 0.;
	inverseProject[ 10 ] = 0.;
	inverseProject[ 11 ] = 1.0 / d;
	inverseProject[ 12 ] = 0.;
	inverseProject[ 13 ] = 0.;
	inverseProject [14 ] = 1.0 / e;
	inverseProject[ 15 ] = -c / (d * e);
	*/

	// inverse of an orthographic transform
	inverseProject[ 0 ] = m_aspectRatioWidth * m_zoomFactor;
	inverseProject[ 1 ] = 0.0f;
	inverseProject[ 2 ] = 0.0f;
	inverseProject[ 3 ] = 0.0f;
	inverseProject[ 4 ] = 0.0f;
	inverseProject[ 5 ] = m_aspectRatioHeight * m_zoomFactor;
	inverseProject[ 6 ] = 0.0f;
	inverseProject[ 7 ] = 0.0f;
	inverseProject[ 8 ] = 0.0f;
	inverseProject[ 9 ] = 0.0f;
	inverseProject[ 10 ] = -(m_far - m_near) / 2.0f;
	inverseProject[ 11 ] = (m_far + m_near)/ 2.0f;
	inverseProject[ 12 ] = 0.0f;
	inverseProject[ 13 ] = 0.0f;
	inverseProject [14 ] = 0.0f;
	inverseProject[ 15 ] = 1.0f;
}

void ChartWidget::paintGL()
{
	setProjectionMatrix( width(), height(), m_zoomFactor );
	setModelViewMatrix();
	draw();

	/* tried this to get the zoomed in viewport to work
	// create a small viewport zoomed in on the mouse location
	glScissor( 0, 0, 25, 25 );
	glEnable(GL_SCISSOR_TEST);
	glClear(GL_DEPTH_BUFFER_BIT);
	glDisable(GL_SCISSOR_TEST);

	// push the zoomed in viewport orthographic projection
	glViewport( 0, 0, 25, 25 );
	glMatrixMode( GL_PROJECTION );
	glPushMatrix();
	glLoadIdentity();
	glOrtho( -0.5f, 0.5f, -0.5f, 0.5f, 0.0f, 0.1f );

	glMatrixMode(GL_MODELVIEW);
	draw();

	// pop the zoomed in viewport orthographic projection
	glMatrixMode( GL_PROJECTION );
	glPopMatrix();
	*/
}

void ChartWidget::draw()
{
	glColor3f( 1.0f, 1.0f, 1.0f );

	// draw the horizontal axis
	glBegin(GL_LINES);
	glColor3f( 1.0f, 1.0f, 1.0f );
	glVertex2d( 0., 0. );
	glVertex2d( m_xDomain, 0. );
	glEnd();

	// draw the vertical axis
	glBegin(GL_LINES);
	glColor3f( 1.0f, 1.0f, 1.0f );
	glVertex2d( 0, -m_yDomain * 0.5 );
	glVertex2d( 0, m_yDomain * 0.5 );
	glEnd();

	// draw the horizonal ticks on the X axis
	int ii = 0;
	for ( ii=1; ii<m_numTicks; ii++ )
	{
		glBegin(GL_LINES);
		glVertex2d( m_xTickStep * ii, -0.1 );
		glVertex2d( m_xTickStep * ii, 0.1 );
		glEnd();
	}

	/*  too slow to use
	// draw a vertical bar at the mouse location
	glBegin(GL_LINES);
	glVertex2d( m_verticalBar, -m_yDomain * 0.4 );
	glVertex2d( m_verticalBar, m_yDomain * 0.4 );
	glEnd();
	*/

	// highlight current and last peaks and valleys
	glPointSize( 10.0f );
	glBegin(GL_POINTS);
	if ( m_peakX != 0.0f || m_peakY != 0.0f )
		glVertex2d( m_peakX, m_peakY );
	if ( m_lastPeakX != 0.0f || m_lastPeakY != 0.0f )
		glVertex2d( m_lastPeakX, m_lastPeakY );

	glColor3f( 1.0f, 0.0f, 0.0f );
	if ( m_valleyX != 0.0f || m_valleyY != 0.0f )
		glVertex2d( m_valleyX, m_valleyY );
	if ( m_lastValleyX != 0.0f || m_lastValleyY != 0.0f )
		glVertex2d( m_lastValleyX, m_lastValleyY );
	glEnd();


	// draw the signals read so far
	for ( ii=0; ii<m_vectorSignals.size(); ii++ )
	{
		switch ( ii ) {
		case 0:
			glColor3f( 1.0f, 0.0f, 0.0f );  //red
			break;
		case 1:
			glColor3f( 0.0f, 1.0f, 0.0f );  // green
			break;
		case 2:
			glColor3f( 0.0f, 0.0f, 1.0f );  // blue
			break;
		case 3:
			glColor3f( 1.0f, 1.0f, 0.0f );  // yellow
			break;
		case 4:
			glColor3f( 1.0f, 0.0f, 1.0f );  // purple
			break;
		case 5:
			glColor3f( 1.0f, 0.5f, 0.0f );  // orange
			break;
		case 6:
			glColor3f( 0.0f, 1.0f, 1.0f );  // cyan
			break;
		default:
			break;
		}

		const QVector< float > *pSignal = &m_vectorSignals.at( ii );
		int count = pSignal->size();
		if ( count < 2 )
			continue;

		if ( m_smoothOn )
		{
			glEnable( GL_LINE_SMOOTH );
			glHint( GL_LINE_SMOOTH_HINT, GL_NICEST );
			glEnable(GL_BLEND);
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			glBegin( GL_LINE_STRIP );
		}
		else
			glBegin(GL_LINES);
		for ( int jj=0; jj<count; jj++ )
		{
			glVertex2d( m_xStep * jj,
						pSignal->at( jj ) * m_vectorScales.at( ii ) );
		}

		glEnd();
	}
}  // end draw

/* -- code for managing the display ends here ----------------------------------*/


void ChartWidget::mousePressEvent(QMouseEvent *event)
{
	if ( !m_vectorScales.count() )
		return;

	lastPos = event->pos();

	// don't handle panning or zooming using the control key
	if ( event->modifiers().testFlag( Qt::ControlModifier ) )
		return;

	// clear the peak or valley widgets and start recording
	if ( event->buttons() &
		Qt::LeftButton )
	{
		// clear peak widgets
		emit qtsignalStartRecordingPeakValues( true );
		m_recordingPeak = true;
		m_currentPeak =  0.0f;
	}

	else if ( event->buttons() &
			  Qt::RightButton )
	{
		// clear valley widgets
		emit qtsignalStartRecordingPeakValues( false );
		m_recordingValley = true;
		m_currentValley =  0.0f;
	}
}

void ChartWidget::mouseReleaseEvent(QMouseEvent * event)
{
	if ( !m_vectorScales.count() )
		return;

	// don't handle panning or zooming using the control key
	if ( event->modifiers().testFlag( Qt::ControlModifier ) )
	{
		m_recordingPeak = false;
		m_recordingValley = false;
		return;
	}

	// turn off recording peak or valley values until the next recording starts
	// emit qtsignalStopRecordingPeakValues();
	if ( m_recordingPeak )
	{
		if ( m_currentPeak == 0.0f )
		{
			// recording should be off at this point
			m_recordingPeak = false;
			return;
		}

		refineMaximum( 0, m_peakTime, m_currentPeak, true );
		emit qtsignalUpdatePeakValue( m_currentPeak, m_peakTime );

		// highlight the peak
		highlightPeak( 0, m_peakTime, m_currentPeak );

		// turn off the update of the last peak until we start recording a peak again
		m_recordingPeak = false;
	}

	if ( m_recordingValley )
	{
		if ( m_currentValley == 0.0f )
		{
			// recording should be off at this point
			m_recordingValley = false;
			return;
		}

		refineMaximum( 0, m_peakTime, m_currentValley, false );
		emit qtsignalUpdatePeakValue( m_currentValley, m_peakTime );

		// highlight the valley
		highlightValley( 0, m_peakTime, m_currentValley );

		// turn off the update of the last valley until we start recording a valley again
		m_recordingValley = false;
	}
}

void ChartWidget::mouseMoveEvent(QMouseEvent *event)
{
	if ( !m_vectorScales.count() )
		return;

	int dx = event->x() - lastPos.x();
	int dy = event->y() - lastPos.y();

	if ( event->modifiers().testFlag( Qt::ControlModifier ) )
	{
		if ( event->buttons() &
			Qt::LeftButton )
		{
			// zoom
			m_zoomFactor += (float) dy / height();
			if ( m_zoomFactor < 0.1f )
				m_zoomFactor = 0.1f;
			setProjectionMatrix( width(), height(), m_zoomFactor );
			update();
			lastPos = event->pos();
			return;
		}

		else if ( event->buttons() &
				  Qt::RightButton )
		{
			// pan
			m_xPan += (float) dx / width();
			m_yPan += (float) -dy / height();
			update();
			lastPos = event->pos();
			return;
		}
	}  // end if pressing control key

	// update the values on the LCD widgets
	updateSignalValues( event->x() );
}

void ChartWidget::keyPressEvent(QKeyEvent *event)
{
	if ( event->modifiers().testFlag( Qt::ControlModifier ) )
	{
	  if ( event->key() == Qt::Key_S )
	  {
		// toggle smoothing
		m_smoothOn = !m_smoothOn;
		update();
	  }

	  // used only for testing
	  else if ( event->key() == Qt::Key_1 )
		  highlightSelectedDataPoint( 1 );

	  else if ( event->key() == Qt::Key_2 )
		  highlightSelectedDataPoint( 2 );

	  else if ( event->key() == Qt::Key_3 )
		  highlightSelectedDataPoint( 3 );

	  else if ( event->key() == Qt::Key_4 )
		  highlightSelectedDataPoint( 4 );

	  else if ( event->key() == Qt::Key_5 )
		  highlightSelectedDataPoint( 5 );

	  return;
	}

	// used to advance forward or backward in time by one data point
	if ( event->key() == Qt::Key_Left ||
		 event->key() == Qt::Key_Right )
	{
		if ( event->key() == Qt::Key_Left )
		{
			// don't change the time to less than 0
			if ( m_timeAtMouse <= 0 )
				return;
			m_timeAtMouse--;
		}

		else
		{
			// right arrow key
			// don't change the time to more than the maximum time
			if ( m_timeAtMouse >= m_numDataPoints - 1 )
				return;
			m_timeAtMouse++;
		}

		int count = m_vectorScales.count();
		for ( int ii=0; ii<count; ii++ )
		{
		  const QVector< float > *pSignal = &m_vectorSignals.at( ii );
		  emit qtsignalUpdateValue( ii, pSignal->at( m_timeAtMouse ), m_timeAtMouse );
		}
	}

	// used to move the current amplitude and time to the first signal peak widget
	if ( event->key() == Qt::Key_Return )
	{
		const QVector< float > *pSignal = &m_vectorSignals.at( 0 );
		emit qtsignalDisplayArbitraryDeltas( pSignal->at( m_timeAtMouse ), m_timeAtMouse );
	}

	QWidget::keyPressEvent( event );
}

// used only for testing
void ChartWidget::highlightSelectedDataPoint( int signal )
{
	Q_ASSERT( signal <= m_vectorScales.count() );

	// find the closest signal data point
	// we know the number of data points and the size of the X domain
	const QVector< float > *pSignal = &m_vectorSignals.at( signal - 1 );
	int count = pSignal->size();
	if ( count < 2 )
		return;

	float xNDC = 2. * (float) lastPos.x() / (float) width() - 1.,
			yNDC = 2. * (float) lastPos.y() / (float) height();

	// Y screen coord increases going down so reverse it
	yNDC = 1. - yNDC;

	// transform from NDC to model coords
	float x = xNDC * m_screenToModel[0] + m_screenToModel[3];
	/*
			y = yNDC * m_screenToModel[5] + m_screenToModel[7],
			z = m_screenToModel[11],
			w = 1.;
	*/

	// start w/ the visible portion of the range in case the user has
	// zoomed in
	float maxNDC = m_screenToModel[0] + m_screenToModel[3],
			minNDC = -m_screenToModel[0] + m_screenToModel[3];
	int highIndex = qMin( (int) (maxNDC/m_xStep), m_numDataPoints ),
			lowIndex = qMax( (int) (minNDC/m_xStep), 0 );

	float dataX = m_xStep * highIndex;
	while ( // fabs( dataX - x ) > 1.0e-3 &&
			lowIndex < highIndex &&
			highIndex - lowIndex > 1 )
	{
		int testIndex = ( highIndex - lowIndex ) >> 1;
		dataX = m_xStep * ( lowIndex + testIndex );
		qDebug() << "low: " << lowIndex << " high: " << highIndex << " dataX: " << dataX;
		if ( x < dataX )
			highIndex = lowIndex + testIndex;
		else
			lowIndex += testIndex;
	};

	// draw a point at the selected location
	float signalValue = pSignal->at( lowIndex );
	m_peakX = dataX;
	m_peakY = signalValue  * m_vectorScales.at( signal - 1 );

	emit qtsignalUpdateValue( signal - 1, signalValue, lowIndex );
}  // end highlightSelectedDataPoint


// updates the signal values on the widgets listening to qtsignalSetSignalValue
void ChartWidget::updateSignalValues( int screenX )
{
	int count = m_vectorScales.count();
	if ( !count )
		return;

	// get the closest signal index corr to the given X screen coord
	m_timeAtMouse = getSignalIndex( screenX );
	if ( m_timeAtMouse < 0 ||
		 m_timeAtMouse > m_numDataPoints - 1 )
	{
		// the call to getSignalIndex() failed
		m_timeAtMouse = 0;
		return;
	}

	for ( int ii=0; ii<count; ii++ )
	{
		const QVector< float > *pSignal = &m_vectorSignals.at( ii );
		int count = pSignal->size();
		if ( count < 2 )
			continue;

		float signalValue = pSignal->at( m_timeAtMouse );
		emit qtsignalUpdateValue( ii, signalValue, m_timeAtMouse );

		// record peaks and valleys
		if ( m_recordingPeak &&
			 ii == 0 )
		{
			if ( m_currentPeak == 0.0f ||
				 signalValue > 	m_currentPeak )
			{
				m_currentPeak = signalValue;
				m_peakTime = m_timeAtMouse;
			}
		}

		if ( m_recordingValley &&
			 ii == 0 )
		{
			if ( m_currentValley == 0.0f ||
				 signalValue < 	m_currentValley )
			{
				m_currentValley = signalValue;
				m_peakTime = m_timeAtMouse;
			}
		}
	}
}  // end updateSignalValues


// returns the index into the signal vectors corr to the given X screen coord
int ChartWidget::getSignalIndex( int screenX )
{
	if ( !m_vectorScales.count() )
		return 0;

	// transform screen coordinates to NDC
	/*
	float screenWidth = width(),
			screenHeight = height();
	*/
	float xNDC = 2. * (float) screenX / (float) width() - 1.;

	// transform from NDC to model coords
	float x = xNDC * m_screenToModel[0] + m_screenToModel[3];

	// find the data point corr to the screen location
	// start w/ the visible portion of the range in case the user has
	// zoomed in
	float maxNDC = m_screenToModel[0] + m_screenToModel[3],
			minNDC = -m_screenToModel[0] + m_screenToModel[3];
	int highIndex = qMin( (int) (maxNDC/m_xStep), m_numDataPoints ),
			lowIndex = qMax( (int) (minNDC/m_xStep), 0 );

	float dataX = m_xStep * highIndex;
	while ( // fabs( dataX - x ) > 1.0e-3 &&
			// lowIndex < highIndex &&
			highIndex - lowIndex > 1 )
	{
		int testIndex = ( highIndex - lowIndex ) >> 1;
		dataX = m_xStep * ( lowIndex + testIndex );
		qDebug() << "low: " << lowIndex << " high: " << highIndex << " dataX: " << dataX;
		if ( x < dataX )
			highIndex = lowIndex + testIndex;
		else
			lowIndex += testIndex;
	};

	if ( lowIndex < 0 )
		lowIndex = 0;
	if ( lowIndex >= m_numDataPoints )
		lowIndex = m_numDataPoints - 1;
	return lowIndex;
}  // end getSignalIndex



// refine the peak and time by searching for the highest value in the vicinity of the given time
void ChartWidget::refineMaximum( int signalId, int &time, float &signal, bool bMaximum ) const
{
	Q_ASSERT( signalId <= m_vectorSignals.count() );

	// search vicinity data points to the right and to the left
	int vicinity = 25;
	const QVector< float > *pSignal = &m_vectorSignals.at( signalId );
	float maxSignal = pSignal->at( time );
	int maxTime = time;
	int startTime = qMax( 0, time - vicinity ),
			endTime = qMin( time + vicinity, pSignal->size() );
	for ( int ii=startTime; ii<endTime; ii++ )
	{
		float testSignal = pSignal->at( ii );
		if ( bMaximum )
		{
			if ( testSignal > maxSignal )
			{
				maxSignal = testSignal;
				maxTime = ii;
			}
			continue;
		}

		// we're searching for a minimum
		if ( testSignal < maxSignal )
		{
			maxSignal = testSignal;
			maxTime = ii;
		}
	}

	// we should have a maximum
	maxSignal = pSignal->at( maxTime );
	if ( bMaximum )
		Q_ASSERT( maxSignal >= signal );
	else
		Q_ASSERT( maxSignal <= signal );
	time = maxTime;
	signal = maxSignal;
}  // end refineMaximum


// highlight the current and last peaks
void ChartWidget::highlightPeak( int signalIndex, int time, double signal )
{
	if ( m_recordingPeak )
	{
		// the current peak will now be the last peak
		m_lastPeakX = m_peakX;
		m_lastPeakY = m_peakY;
	}

	m_peakX = time * m_xStep;
	m_peakY = signal * m_vectorScales.at( signalIndex );
}

// highlight the current and last valleys
void ChartWidget::highlightValley( int signalIndex, int time, double signal )
{
	if ( m_recordingValley )
	{
		// the current valley will now be the last valley
		m_lastValleyX = m_valleyX;
		m_lastValleyY = m_valleyY;
	}

	m_valleyX = time * m_xStep;
	m_valleyY = signal * m_vectorScales.at( signalIndex );
}

