using UnityEngine;
using UnityEngine.UI;
using System.Collections;
using System.Runtime.InteropServices;
using System.Threading;
using System.IO;

public class UseRenderingPlugin : MonoBehaviour {
	
	int width = 1024;
	int height = 576;
//	int width = 1280;
//	int height = 7;
	Texture2D tex;
	int startCount=0;
	int awakeCount = 0;
	Camera movingCamera;
	Color[] blackTexture;
	public GameObject sys;
	private MainScript mainScript;
	
	Color[] correctSelectionIndicator;
	Color[] incorrectSelectionIndicator;
	
	[DllImport ("UnityServerPlugin")]
	private static extern void SetTextureFromUnity (System.IntPtr texture);

	[DllImport ("UnityServerPlugin")]
	private static extern void setLog ();
	
	[DllImport ("UnityServerPlugin")]
	private static extern void endServer ();
	
	[DllImport ("UnityServerPlugin")]
	private static extern void SetTimeFromUnity (float t);

    [DllImport("UnityServerPlugin")]
    private static extern void SetCubeRenderTextureFromUnity(System.IntPtr cubeTexture, out System.IntPtr pixels);
	
	void Awake()
	{
		setLog ();
		
		
		
		Texture2D ci =  (Texture2D)Resources.Load("CorrectSelection", typeof(Texture2D));
		Texture2D ii =  (Texture2D)Resources.Load("IncorrectSelection", typeof(Texture2D));
		
		correctSelectionIndicator = ci.GetPixels ();
		incorrectSelectionIndicator = ii.GetPixels();
		
		blackTexture = new Color[width*height];
		for(int i =0; i< width*height; i++)
		{
			blackTexture[i] = Color.black;	
		}
		
		sys = GameObject.Find("System");
		mainScript = sys.GetComponent<MainScript>();
		
		/*ServerThread server = new ServerThread();
		Thread st = new Thread(new ThreadStart(server.startServer));
		st.Start();
		
		Debug.Log("Started Server");
		
		OSCPhaseSpaceThread oscPSClient = new OSCPhaseSpaceThread();
		Thread oscPSt = new Thread(new ThreadStart(oscPSClient.startServer));
		oscPSt.Start();
		
		Debug.Log("Started OSC Client");
		
		OSCThread oscClient = new OSCThread();
		Thread osct = new Thread(new ThreadStart(oscClient.startServer));
		osct.Start();*/
		
	}
	
	public float posX = 512f; //Position the DrawTexture command while testing.
	public float posY = 512f; 
	Texture2D ARSelectionTexture;

    private Camera cam;
    private RenderTexture rtexLeft;
    private RenderTexture rtexRight;
    private const int cubemapSize = 512;
    private Texture2D dst;
    private System.IntPtr rtexLeftNativePtr;
	
	//void Start(){
		//Debug.Log ("Started.");
	IEnumerator Start () {
		ARSelectionTexture = Resources.Load("GUISelectionBox") as Texture2D;
		CreateTextureAndPassToPlugin();

        //if (!cam)
        //{
            //var go = new GameObject("MonoCamera");
            //go.hideFlags = HideFlags.HideAndDontSave;
            //go.transform.position = transform.position;
            //go.transform.rotation = Quaternion.identity;
            //cam = go.GetComponent<Camera>();;
            //cam.farClipPlane = 2000; // don't render very far into cubemap
            //cam.enabled = false;
            var go = GameObject.Find("Cam1");
            var test = go.GetComponents<Camera>();
            cam = go.GetComponent<Camera>();
        //}

        //if (!rtexLeft)
        //{
            rtexLeft = new RenderTexture(cubemapSize, cubemapSize, 16/*, RenderTextureFormat.ARGBInt*/);
            rtexLeft.isCubemap = true;
            rtexLeft.hideFlags = HideFlags.HideAndDontSave;
            rtexLeft.wrapMode = TextureWrapMode.Repeat;
            
            //GetComponent<Renderer>().sharedMaterial.SetTexture ("_Cube", rtexLeft);
        //}

        //if (!rtexRight)
        //{
            rtexRight = new RenderTexture(cubemapSize, cubemapSize, 16);
            rtexRight.isCubemap = true;
            rtexRight.hideFlags = HideFlags.HideAndDontSave;
        //}

        //if (!dst)
        //{
            dst = new Texture2D(cubemapSize, cubemapSize, TextureFormat.ARGB32, false);
        //}


            var cube = GameObject.Find("CubeMapTestCube");
            Renderer renderer = cube.GetComponent<Renderer>();
            Material material = renderer.sharedMaterial;
            //material.mainTexture = rtexLeft;
            material.SetTexture("_Cube", rtexLeft);

            GameObject.Find("CubeMapTestCube2").GetComponent<Renderer>().sharedMaterial.SetTexture("_Cube", rtexRight);
            

		yield return StartCoroutine("CallPluginAtEndOfFrames");
	}
	
	void OnApplicationQuit(){
		PlayerPrefs.Save ();
		//endServer ();
		
	}
	
	bool drawIndicator = false;
	float startTime;
	public void drawSelectionIndication()
	{
		drawIndicator = true;
		startTime = Time.time; // set start time for timer
	}

	private void CreateTextureAndPassToPlugin()
	{
		movingCamera =  GameObject.Find("MonoCamera").GetComponent<Camera>();//Camera.allCameras[2];
		//movingCamera =  GameObject.Find("Camera").camera;//Camera.allCameras[2]; //**This is the stereo renderer
		
		
		//movingCamera.rect = new Rect(0,0,width,height);
		// Create a texture
		tex = new Texture2D(width,height,TextureFormat.RGB24,false);
		
		// Set point filtering just so we can see the pixels clearly
		//tex.filterMode = FilterMode.Point;

		// Call Apply() so it's actually uploaded to the GPU
		tex.Apply();

		// Set texture onto our matrial
		//renderer.material.mainTexture = tex;

		// Pass texture pointer to the plugin
		SetTextureFromUnity (tex.GetNativeTexturePtr());


	}
	int num=0;


    byte[] managedPixels = new byte[cubemapSize * cubemapSize * 4];

    void Update()
    {
        

        cam.RenderToCubemap(rtexLeft);
        cam.RenderToCubemap(rtexRight);
        //cam.RenderToCubemap(rtexRight);
        /*RenderTexture x = new RenderTexture(cubemapSize, cubemapSize, 16);
        cam.targetTexture = x;
        cam.Render();*/

        /*RenderTexture x = rtexLeft;

        var tex = new Texture2D(cubemapSize, cubemapSize, TextureFormat.RGB24, false);
        RenderTexture.active = x;
        Texture2D virtualPhoto = new Texture2D(cubemapSize, cubemapSize, TextureFormat.RGB24, false);
        virtualPhoto.ReadPixels(new Rect(0, 0, cubemapSize, cubemapSize), 0, 0); // you get the center section
        virtualPhoto.Apply();*/
        
        
        //RenderBuffer b = rtexLeft.colorBuffer;
        // Read screen contents into the texture        
        //tex.SetPixels(rtexLeft.GetPixels(CubemapFace.PositiveX));

        /*if (rtexLeftNativePtr == System.IntPtr.Zero)
        {
            rtexLeftNativePtr = rtexLeft.GetNativeTexturePtr();
        }*/
        

        //System.IntPtr unmanagedPixels = System.IntPtr.Zero;
        //SetCubeRenderTextureFromUnity(rtexLeftNativePtr, out unmanagedPixels);
        
        //Marshal.Copy(unmanagedPixels, managedPixels, 0, cubemapSize * cubemapSize * 4);

        //dst.LoadRawTextureData(managedPixels);
        //dst.Apply();

        //RawImage testRenderImage = GameObject.Find("TestRenderImage").GetComponent<RawImage>();
        //testRenderImage.texture = virtualPhoto;// dst;

        
    }
	
	private IEnumerator CallPluginAtEndOfFrames ()
	{
		
		while (true) {
			if(Input.GetKeyDown("f"))
			{
				Screen.fullScreen = !Screen.fullScreen;	
			}
			if(Input.GetKeyDown ("q"))
			{
				endServer ();
				Application.Quit ();
			}
			

			// Wait until all frame rendering is done
			yield return new WaitForEndOfFrame();
			
			//Need to call this (or any plugin function) to keep calling native rendering events
			SetTextureFromUnity (tex.GetNativeTexturePtr());
			
			//movingCamera.Render();
            RenderTexture.active = movingCamera.targetTexture;
			
//			if(mainScript.getExperimentInterface() == (int)MainScript.interfaces.Pointing)
//			{
				//tex.SetPixels(blackTexture);
//			}
//			else
//			{
				tex.ReadPixels(new Rect(0, 224 /*192*/, width, height), 0, 0);
				//tex.Apply ();
			//}
			
//			if(drawIndicator)
//			{
//				
//				tex.SetPixels (0,0,64,64,correctSelectionIndicator);
//			}
			
			//tex.ReadPixels(new Rect(0, 0, width, height), 0, 0);
			RenderTexture.active = null;
			//if(drawIndicator)
				
			
			
//			
//			RenderTexture.active = movingCamera.targetTexture;
//				//RenderTexture.active = cam3.targetTexture; //Set my RenderTexture active so DrawTexture will draw to it.
//				GL.PushMatrix (); //Saves both projection and modelview matrices to the matrix stack.
//				GL.LoadPixelMatrix (0, 1024, 1024, 0); //Setup a matrix for pixel-correct rendering.
//				//Draw my stampTexture on my RenderTexture positioned by posX and posY.
//				Graphics.DrawTexture (new Rect (posX - ARSelectionTexture.width / 2, (1024 - posY) - ARSelectionTexture.height / 2, ARSelectionTexture.width, ARSelectionTexture.height), ARSelectionTexture);
//				GL.PopMatrix (); //Restores both projection and modelview matrices off the top of the matrix stack.
//				RenderTexture.active = null; //De-activate my RenderTexture.
			tex.Apply ();
			
			
			
			//var bytes = tex.EncodeToPNG();
			//File.WriteAllBytes(Application.dataPath + "/../SavedScreen" + num + ".png", bytes);
			//num++;
			
			GL.IssuePluginEvent (1);
		}
	}
}