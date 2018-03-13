﻿using System;

using UnityEngine;

using HoloToolkit.Unity.InputModule;
using HoloToolkit.UI.Keyboard;

using TMPro;

public class InfoEventManager : MonoBehaviour, IInputClickHandler
{
	private InformationManager informationManager;
	private TextMeshPro selectedField = null;

	void Awake() {
		informationManager = this.transform.root.GetComponent<InformationManager>();
		Keyboard.Instance.OnTextSubmitted += OnKeyboardSubmitted;
		Keyboard.Instance.OnClosed += OnKeyboardClosed;
	}

	private void OnKeyboardClosed(object sender, EventArgs e)
	{
		// It is really important to unsubscribe to these events as soon as the text is submitted/keyboard closed
		// otherwise modifications will be propagated to the other edited fields.
		Keyboard.Instance.OnTextUpdated -= OnKeyboardTextUpdated;
		Keyboard.Instance.OnClosed -= OnKeyboardClosed;
		Keyboard.Instance.Close();
	}

	private void OnKeyboardSubmitted(object sender, EventArgs e)
	{
		// It is really important to unsubscribe to these events as soon as the text is submitted/keyboard closed 
		// otherwise modifications will be propagated to the other edited fields.
		Keyboard.Instance.OnTextUpdated -= OnKeyboardTextUpdated;
		Keyboard.Instance.OnClosed -= OnKeyboardClosed;
	}

	private void OnKeyboardTextUpdated(string content)
	{
		if (!string.IsNullOrEmpty(content))
		{
			selectedField.text = content;
			// This line should go in the OnKeyboardSubmitted but atm there is a bug
			// causing the event to be thrown too many times and the label ends up resetting for no reasons.
			informationManager.InformationsChanged();
		}
	}

	public void OnInputClicked(InputClickedEventData eventData)
	{
		if (eventData.selectedObject == null) {
			return;
		}

		string editField = eventData.selectedObject.name;
		Keyboard.LayoutType keyboardLayout = Keyboard.LayoutType.Alpha;
		switch (editField) { 
			case "Label":
				selectedField = informationManager.Label;
				break;
			case "Author":
				selectedField = informationManager.Author;
				break;
			case "Date":
				keyboardLayout = Keyboard.LayoutType.Symbol;
				selectedField = informationManager.Date;
				break;
			case "Description":
				selectedField = informationManager.Description;
				break;
		}

		// We need to subscribe to this event after we set the variable currentField otherwise it is not working (???)
		Keyboard.Instance.OnTextUpdated += OnKeyboardTextUpdated;
		Keyboard.Instance.PresentKeyboard(selectedField.text, keyboardLayout);
	}
}