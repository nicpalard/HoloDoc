﻿using HoloToolkit.Unity.InputModule;
using System;
using System.Collections;
using System.Collections.Generic;

using UnityEngine;

public class DocumentManager : MonoBehaviour, IFocusable {

	[HideInInspector]
	public DocumentProperties	Properties;
	public GameObject			OutlineQuad;

	private GameObject			docPreview;
	private GameObject			docInformations;
	private GameObject			docButtons;
	private GameObject			docBackground;
	private Material			material;
	private DocumentAnimator	animator;

	private bool clicked;

	void Awake() {
		this.docPreview = transform.Find("Preview").gameObject;

		this.docInformations = transform.Find("Informations").gameObject;
		this.docInformations.SetActive(false);

		this.docButtons = transform.Find("Buttons").gameObject;
		this.docButtons.SetActive(false);

		this.Properties = new DocumentProperties();

		this.animator = transform.GetComponent<DocumentAnimator>();
	}

	void Start() {
		this.docInformations.GetComponent<InformationManager>().SetProperties(this.Properties);
		this.docInformations.GetComponent<InformationManager>().OnInformationModified += DocumentInformationsModifiedHandler;

		StartCoroutine(WaitForInstantiate());
	}

	IEnumerator WaitForInstantiate() {
		yield return new WaitForSeconds(0.1f);
		DocumentCollection.Instance.SetFocusedDocument(this.gameObject);
	}

	public void SetPosition(Vector3 position) {
		this.transform.position = position;
	}

	public void SetPhoto(Texture2D photo) {
		float finalHeight, finalWidth;
		if (photo.height > photo.width)	{
			finalHeight = 1.35f;
			finalWidth = ((float)photo.width / (float)photo.height) * 1.35f;
		}
		else {
			finalHeight = ((float)photo.height / (float)photo.width) * 1.35f;
			finalWidth = 1.35f;
		}

		this.docPreview.transform.localScale = new Vector3(finalWidth, finalHeight, 1);
		this.docPreview.transform.GetComponent<Renderer>().material.mainTexture = photo;

		Resolution resolution = new Resolution() {
			width = photo.width,
			height = photo.height
		};

		CameraFrame cameraFrame = new CameraFrame(resolution, photo.GetPixels32());
		this.Properties.Photo = cameraFrame;
	}

	public void SetColor(Color color) {
		this.OutlineQuad.SetActive(true);
		this.OutlineQuad.GetComponent<Renderer>().material.color = color;
		UpdateLinkDisplay();
	}

	private void DocumentInformationsModifiedHandler(string author, string date, string description, string label) {
		this.Properties.Author = author;
		this.Properties.Date = date;
		this.Properties.Description = description;
		this.Properties.Label = label;
#if USE_SERVER
		RequestLauncher.Instance.UpdateDocument(this.Properties, OnUpdateDocument);
#endif
	}

    // TODO: Implement this function
    private void OnUpdateDocument(RequestLauncher.RequestAnswerDocument item, bool success)
    {
        if (String.IsNullOrEmpty(item.Error))
        {
            this.Properties = new DocumentProperties(this.Properties)
            {
                Label = item.Label,
                Description = item.Desc,
                Author = item.Author,
            };
        }
    }

        public void OnFocusEnter() {
		this.animator.ZoomIn();
	}

	public void OnFocusExit() {
		this.animator.ZoomOut();
	}

	public void ToggleFocus() {
		this.docInformations.SetActive(!docInformations.activeInHierarchy);
		this.docButtons.SetActive(!docButtons.activeInHierarchy);
		UpdateLinkDisplay();
		this.animator.Animate();
	}

	public void UpdateLinkDisplay() {
		this.docInformations.transform.Find("LinkPreview/DocPreview1").gameObject.SetActive(false);
		this.docInformations.transform.Find("LinkPreview/DocPreview2").gameObject.SetActive(false);
		this.docInformations.transform.Find("LinkPreview/DocPreview3").gameObject.SetActive(false);

		if (this.Properties.LinkId != -1) {
			this.docInformations.transform.Find("LinkPreview/NoLinks").gameObject.SetActive(false);
			uint linkCount = 0;
			List<GameObject> objects = LinkManager.Instance.GetObjects(this.Properties.LinkId);
			foreach (GameObject go in objects) {
				if (go != this.gameObject) {
					linkCount++;
					if (linkCount > 3) { break; }
					GameObject preview = this.docInformations.transform.Find("LinkPreview/DocPreview" + linkCount).gameObject;
					preview.SetActive(true);
					preview.GetComponent<Renderer>().material.mainTexture = go.transform.Find("Preview").gameObject.GetComponent<Renderer>().material.mainTexture;
				}
			}
		}
		else {
			this.docInformations.transform.Find("LinkPreview/NoLinks").gameObject.SetActive(true);
		}
	}

	public void Toggle() {
		this.docPreview.SetActive(!docPreview.activeInHierarchy);
	}

	public void UpdatePhoto() {
		DocumentCollection.Instance.Toggle();
		GlobalActions.Instance.UpdateDocumentPhoto(this.gameObject);
	}

	public void StartLink() {
		LinkManager.Instance.OnLinkStarted(this.gameObject);
	}

	public void EndLink() {
		LinkManager.Instance.OnLinkEnded(this.gameObject);
	}

	public void OnLinkBreak() {
		this.OutlineQuad.SetActive(false);
		UpdateLinkDisplay();
		// NOTE: Maybe put a default color.
		//RequestLauncher.Instance.BreakLink(this.Properties.Id);
	}

	public void Open() {
		if (DocumentCollection.Instance.IsFocused(this.gameObject)) {
			return;
		}
		DocumentCollection.Instance.SetFocusedDocument(this.gameObject);
	}

	public void Close() {
		if (!DocumentCollection.Instance.IsFocused(this.gameObject)) {
			return;
		}
		DocumentCollection.Instance.SetFocusedDocument(this.gameObject);
	}
}