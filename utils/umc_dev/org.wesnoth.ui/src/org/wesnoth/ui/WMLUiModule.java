/*******************************************************************************
 * Copyright (c) 2010 - 2011 by Timotei Dolean <timotei21@gmail.com>
 *
 * This program and the accompanying materials are made available
 * under the terms of the Eclipse Public License v1.0
 * which accompanies this distribution, and is available at
 * http://www.eclipse.org/legal/epl-v10.html
 *******************************************************************************/
package org.wesnoth.ui;

import org.eclipse.jface.text.hyperlink.IHyperlinkDetector;
import org.eclipse.jface.text.source.ICharacterPairMatcher;
import org.eclipse.jface.viewers.ILabelProvider;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.eclipse.xtext.resource.ILocationInFileProvider;
import org.eclipse.xtext.resource.containers.IAllContainersState;
import org.eclipse.xtext.ui.editor.IXtextEditorCallback;
import org.eclipse.xtext.ui.editor.XtextEditor;
import org.eclipse.xtext.ui.editor.autoedit.AbstractEditStrategyProvider;
import org.eclipse.xtext.ui.editor.contentassist.ContentAssistContext;
import org.eclipse.xtext.ui.editor.contentassist.ICompletionProposalComparator;
import org.eclipse.xtext.ui.editor.folding.DefaultFoldingRegionProvider;
import org.eclipse.xtext.ui.editor.hyperlinking.HyperlinkHelper;
import org.eclipse.xtext.ui.editor.syntaxcoloring.AbstractAntlrTokenToAttributeIdMapper;
import org.eclipse.xtext.ui.editor.syntaxcoloring.HighlightingReconciler;
import org.eclipse.xtext.ui.editor.syntaxcoloring.IHighlightingConfiguration;
import org.eclipse.xtext.ui.editor.syntaxcoloring.IHighlightingHelper;
import org.eclipse.xtext.ui.editor.syntaxcoloring.ISemanticHighlightingCalculator;
import org.wesnoth.ui.autoedit.WMLAutoEditStrategy;
import org.wesnoth.ui.contentassist.WMLContentAssistContext;
import org.wesnoth.ui.contentassist.WMLProposalComparator;
import org.wesnoth.ui.editor.WMLAbstractDirtyEditorCallback;
import org.wesnoth.ui.editor.WMLEditor;
import org.wesnoth.ui.folding.WMLFoldingRegionProvider;
import org.wesnoth.ui.labeling.WMLLabelProvider;
import org.wesnoth.ui.navigation.WMLHyperlinkHelper;
import org.wesnoth.ui.resource.WMLLocationInFileProvider;
import org.wesnoth.ui.syntax.WMLAntlrTokenToAttributeIdMapper;
import org.wesnoth.ui.syntax.WMLCharacterPairMatcher;
import org.wesnoth.ui.syntax.WMLHighlightingConfiguration;
import org.wesnoth.ui.syntax.WMLHighlightingHelper;
import org.wesnoth.ui.syntax.WMLHighlightingReconciler;
import org.wesnoth.ui.syntax.WMLSemanticHighlightingCalculator;

import com.google.inject.Binder;
import com.google.inject.Provider;

/**
 * Use this class to register components to be used within the IDE.
 */
@SuppressWarnings("all")
public class WMLUiModule extends org.wesnoth.ui.AbstractWMLUiModule
{
	public final static boolean DEBUG = false;

	public WMLUiModule(AbstractUIPlugin plugin) {
		super(plugin);
	}

	@Override
	public void configure(Binder binder)
	{
		super.configure(binder);
	}

	public Class<? extends IHighlightingConfiguration> bindIHighlightingConfiguration()
	{
		return WMLHighlightingConfiguration.class;
	}

	public Class<? extends AbstractAntlrTokenToAttributeIdMapper> bindTokenToAttributeIdMapper()
	{
		return WMLAntlrTokenToAttributeIdMapper.class;
	}

	public Class<? extends ISemanticHighlightingCalculator> bindISemanticHighlightingCalculator()
	{
		return WMLSemanticHighlightingCalculator.class;
	}

    @Override
    public ICharacterPairMatcher bindICharacterPairMatcher()
    {
        return new WMLCharacterPairMatcher( new char[] { '(', ')', '{', '}', '[', ']' } );
    }

	public Class<? extends XtextEditor> bindEditor()
	{
		return WMLEditor.class;
	}

	public Class<? extends IHighlightingHelper> bindIHighlightingHelper()
	{
		return WMLHighlightingHelper.class;
	}

	public Class<? extends HighlightingReconciler> bindHighlightingReconciler()
	{
		return WMLHighlightingReconciler.class;
	}

	public Class<? extends ILocationInFileProvider> bindILocationInFileProvider()
	{
		return WMLLocationInFileProvider.class;
	}

	@Override
	public Class<? extends IHyperlinkDetector> bindIHyperlinkDetector()
	{
		return super.bindIHyperlinkDetector();
	}

	public Class<? extends HyperlinkHelper> bindHyperlinkHelper()
	{
		return WMLHyperlinkHelper.class;
	}

	public Class<? extends ContentAssistContext> bindContentAssistContext()
	{
		return WMLContentAssistContext.class;
	}

	public Class<? extends ICompletionProposalComparator> bindICompletionProposalComparator()
	{
		return WMLProposalComparator.class;
	}

	@Override
	public Class<? extends AbstractEditStrategyProvider> bindAbstractEditStrategyProvider()
	{
	    return WMLAutoEditStrategy.class;
	}

	@Override
	public Class<? extends ILabelProvider> bindILabelProvider()
	{
		return WMLLabelProvider.class;
	}

	@Override
	public Class<? extends IXtextEditorCallback> bindIXtextEditorCallback()
	{
		return WMLAbstractDirtyEditorCallback.class;
	}

	public Class<? extends DefaultFoldingRegionProvider> bindDefaultFoldingRegionProvider()
	{
		return WMLFoldingRegionProvider.class;
	}

	@Override
	public Provider<IAllContainersState> provideIAllContainersState()
	{
	    return org.eclipse.xtext.ui.shared.Access.getWorkspaceProjectsState();
	}
}
