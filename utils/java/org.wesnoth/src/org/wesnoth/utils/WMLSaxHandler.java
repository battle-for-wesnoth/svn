/*******************************************************************************
 * Copyright (c) 2010 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.utils;

import java.util.Stack;

import org.xml.sax.Attributes;
import org.xml.sax.SAXException;
import org.xml.sax.helpers.DefaultHandler;

/**
 * A simple wml sax handler that parsers the xml of a config file
 */
public class WMLSaxHandler extends DefaultHandler
{
	public String CampaignId = null;
	public String ScenarioId = null;

	private static Stack<String> stack_;
	public WMLSaxHandler()
	{
		stack_ = new Stack<String>();
	}

    @Override
	public void startElement(String uri, String localName,
       String rawName, Attributes attributes)
    {
          stack_.push(rawName);
    }

    @Override
    public void characters(char[] ch, int start, int length)
    		throws SAXException
    {
    	if (stack_.peek().equals("id"))
    	{
    		if (stack_.get(stack_.size() - 2).equals("campaign"))
    			 CampaignId = new String(ch, start, length);
    		else if (stack_.get(stack_.size() - 2).equals("scenario"))
    			 ScenarioId = new String(ch, start, length);
    	}
    	super.characters(ch, start, length);
    }
    @Override
    public void endElement(String uri, String localName, String qName)
    		throws SAXException
    {
    	super.endElement(uri, localName, qName);
    	stack_.pop();
    }

    @Override
	public void endDocument()
    {
    }
}
