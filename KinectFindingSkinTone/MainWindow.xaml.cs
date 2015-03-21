using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using Microsoft.Kinect;
using Microsoft.Kinect.Face;

namespace KinectFindingSkinTone
{
    /// <summary>
    /// Interaction logic for MainWindow.xaml
    /// </summary>
    public partial class MainWindow : Window
    {
        
        private KinectSensor m_sensor;
        private BodyFrameReader m_bodyReader;
        private HighDefinitionFaceFrameReader m_hdFaceReader;
        private HighDefinitionFaceFrameSource m_hdFaceSource;
        private FaceModel m_faceModel;
        private FaceAlignment m_faceAlignment;
        private FaceModelBuilder m_faceBuilder;
        private ulong m_trackedBodyId;
        private bool m_faceBuilderStarted;
        private bool m_faceBuildComplete;

        public MainWindow()
        {
            InitializeComponent();
            InitializeKinect();
        }

        public void InitializeKinect()
        {
            m_sensor = KinectSensor.GetDefault();
            m_bodyReader = m_sensor.BodyFrameSource.OpenReader();
            m_bodyReader.FrameArrived += m_bodyReader_FrameArrived;
            
            m_hdFaceSource = new HighDefinitionFaceFrameSource(m_sensor);
            m_hdFaceReader = m_hdFaceSource.OpenReader();
            m_hdFaceReader.FrameArrived += m_hdFaceReader_FrameArrived;
            m_faceModel = new FaceModel();
            m_faceBuilder =
                m_hdFaceReader.HighDefinitionFaceFrameSource.OpenModelBuilder(FaceModelBuilderAttributes.HairColor |                    FaceModelBuilderAttributes.SkinColor);
            m_faceBuilder.CollectionCompleted += m_faceBuilder_CollectionCompleted;
            m_faceBuilder.CaptureStatusChanged += m_faceBuilder_CaptureStatusChanged;
            m_faceBuilder.CollectionStatusChanged += m_faceBuilder_CollectionStatusChanged;
            m_faceAlignment = new FaceAlignment();
            m_trackedBodyId = 0;
            m_faceBuilderStarted = false;
            m_faceBuildComplete = false;
            m_sensor.Open();
        }

        void m_faceBuilder_CollectionStatusChanged(object sender, FaceModelBuilderCollectionStatusChangedEventArgs e)
        {
            var collectionStatus = e.PreviousCollectionStatus;
            switch (collectionStatus)
            {
                    case FaceModelBuilderCollectionStatus.Complete:
                    lblCollectionStatus.Text = "CollectionStatus: Complete";
                    m_faceBuildComplete = true;
                    break;
                    case FaceModelBuilderCollectionStatus.FrontViewFramesNeeded:
                    lblCollectionStatus.Text = "CollectionStatus: FrontView Frames Needed";
                    break;
                    case FaceModelBuilderCollectionStatus.LeftViewsNeeded:
                    lblCollectionStatus.Text = "CollectionStatus: LeftView Frames Needed";
                    break;
                    case FaceModelBuilderCollectionStatus.MoreFramesNeeded:
                    lblCollectionStatus.Text = "CollectionStatus: More Frames Needed";
                    break;
                    case FaceModelBuilderCollectionStatus.RightViewsNeeded:
                    lblCollectionStatus.Text = "CollectionStatus: RightView Frames Needed";
                    break;
                    case FaceModelBuilderCollectionStatus.TiltedUpViewsNeeded:
                    lblCollectionStatus.Text = "CollectionStatus: TiltedUpView Frames Needed";
                    break;

            }
        }

        void m_faceBuilder_CaptureStatusChanged(object sender, FaceModelBuilderCaptureStatusChangedEventArgs e)
        {
            var captureStatus = e.PreviousCaptureStatus;
            switch (captureStatus)
            {
                    case FaceModelBuilderCaptureStatus.FaceTooFar:
                    lblStatus.Text = "FrameStatus: Face is too Far";
                    break;
                    case FaceModelBuilderCaptureStatus.FaceTooNear:
                    lblStatus.Text = "FrameStatus: Face is too Close";
                    break;
                    case FaceModelBuilderCaptureStatus.GoodFrameCapture:
                    lblStatus.Text = "FrameStatus: Good Frame Capture";
                    break;
                    case FaceModelBuilderCaptureStatus.LostFaceTrack:
                    lblStatus.Text = "FrameStatus: Lost Face Tracking";
                    break;
                    case FaceModelBuilderCaptureStatus.MovingTooFast:
                    lblStatus.Text = "FrameStatus: Moving Too Fast";
                    break;
                    case FaceModelBuilderCaptureStatus.OtherViewsNeeded:
                    lblStatus.Text = "FrameStatus: Other Views Needed";
                    break;
                    case FaceModelBuilderCaptureStatus.SystemError:
                    lblStatus.Text = "FrameStatus: System Error";
                    break;
                default:
                    lblStatus.Text = "FrameStatus: Unkown";
                    break;

            }
        }

        private void m_faceBuilder_CollectionCompleted(object sender, FaceModelBuilderCollectionCompletedEventArgs e)
        {
            var status = m_faceBuilder.CollectionStatus;
            //var captureStatus = m_faceBuilder.CaptureStatus;
            if (status == FaceModelBuilderCollectionStatus.Complete && m_faceBuildComplete)
            {
                try
                {
                    m_faceModel = e.ModelData.ProduceFaceModel();
                }
                catch (Exception ex)
                {
                    lblCollectionStatus.Text = "Error: " + ex.ToString();
                    lblStatus.Text = "Restarting...";
                    m_faceBuildComplete = false;
                    m_faceBuilderStarted = false;
                    m_sensor.Close();
                    System.Threading.Thread.Sleep(1000);
                    m_sensor.Open();
                    return;
                }
                    var skinColor = UIntToColor( m_faceModel.SkinColor);
                    var hairColor = UIntToColor(m_faceModel.HairColor);
                
                    var skinBrush = new SolidColorBrush(skinColor);

                    var hairBrush = new SolidColorBrush(hairColor);

                    skinColorCanvas.Background = skinBrush;

                    lblSkinColor.Text += " " + skinBrush.ToString();

                hairColorCanvas.Background = hairBrush;
                
                lblHairColor.Text += " " + hairBrush.ToString();


                    m_faceBuilderStarted = false;
                    m_sensor.Close();
               
            }
        }
        private Color UIntToColor(uint color)
        {
            //.Net colors are presented as
            // a b g r
            //instead of
            // a r g b
            byte a = (byte)(color >> 24);
            byte b = (byte)(color >> 16);
            byte g = (byte)(color >> 8);
            byte r = (byte)(color >> 0);
            return Color.FromArgb(250, r, g, b);
        }

        void m_hdFaceReader_FrameArrived(object sender, HighDefinitionFaceFrameArrivedEventArgs e)
        {
            if (!m_faceBuilderStarted)
            {
                m_faceBuilder.BeginFaceDataCollection();
            }
            
        }

        void m_bodyReader_FrameArrived(object sender, BodyFrameArrivedEventArgs e)
        {
            using (var bodyFrame = e.FrameReference.AcquireFrame())
            {
                if (null != bodyFrame)
                {
                    Body[] bodies = new Body[bodyFrame.BodyCount];
                    bodyFrame.GetAndRefreshBodyData(bodies);
                    foreach (var body in bodies)
                    {
                        if (body.IsTracked)
                        {
                            m_trackedBodyId = body.TrackingId;
                            m_hdFaceReader.HighDefinitionFaceFrameSource.TrackingId = m_trackedBodyId;
                        }
                    }
                }
            }
        }
    }
}
